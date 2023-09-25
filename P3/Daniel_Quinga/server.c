#include "proxy.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <err.h>

#define MAX_CLTS 250
#define MAX_TXT 1024

struct Node
{
    struct request rq;
    int id_serv_cl;
    int fd;
    int free_cl;
    struct Node *next;
};
typedef struct Node Node;

struct Params
{
    int port;
    int ratio;
    char priority[MAX_TXT];
};
typedef struct Params Params;

Params params;

int fds[MAX_CLTS*2];
int ids[MAX_CLTS*2];

int counter = 0, ind = -1, priority_rdrs = 1, full_ids = 0;
int n_wrtrs = 0, n_rdrs = 0, n_cl = 0, wait_rdrs = 0, sin_hambruna_rdrs = 0, n_pass_lctr = 0;
int counter_writers = 0, counter_readers = 0, finished_writers = 0, rdrs_finished = 0;

pthread_mutex_t mtx_wrtrs;
pthread_mutex_t mtx_rdrs;
pthread_mutex_t mtx_cl_count;
pthread_mutex_t mtx_list;
pthread_mutex_t mtx_file;
pthread_cond_t cond_rdrs;
pthread_cond_t cond_wrtrs;
sem_t sem;

Node *head_cl = NULL;

int check_nargs(int n) {

    return (n == 4 || n == 6);
}

void save_params(int n, char **args, Params *p) {

    int c;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"port", required_argument, 0, 'p'},
            {"priority", required_argument, 0, 'u'},
            {"ratio", required_argument, 0, 'r'}
        };

        c = getopt_long (n, args, "p:u:r::",
                long_options, &option_index);

        if (c == -1) {
           break;
        }

        switch (c) {
        case 'p':
            p->port = atoi(optarg);
            break;

        case 'u':
            if (strcmp(optarg, "writer") == 0) {
                priority_rdrs = 0;
            }
            strncpy(p->priority, optarg, sizeof(p->priority));
            break;

        case 'r':
            p->ratio = atoi(optarg);
            break;
        }
    }

    if (optind < n) {
        fprintf(stderr,"non-option ARGV-elements: ");
        while (optind < n)
            fprintf(stderr,"%s ", args[optind++]);
        fprintf(stderr,"\n");
        exit(EXIT_FAILURE);
    }
}

int find_slot(int *datas, int max_data) {

    int i;

    for (i = 0; i < max_data; i++) {
        if (datas[i] == -1) {
            return i;
        }
    }

    return -1;
}

void add_cl(Node **head, int fd, struct request rq, pthread_mutex_t *mtx) {

    Node* last;
    Node* new = (Node*) malloc(sizeof(Node));
    new->rq = rq;
    new->fd = fd;
    new->free_cl = 0;
    new->next = NULL;
    
    pthread_mutex_lock(mtx);

    if (ind >= MAX_CLTS*2 || full_ids) {
        ind = find_slot(ids, MAX_CLTS);
        full_ids = 1;
    } else {
        ind++;
    }

    ids[ind] = ind;
    new->id_serv_cl = ids[ind];
    
    if ((*head) == NULL) {
        (*head) = new;
    } else {
        last = (*head);
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = new;
    }
    
    pthread_mutex_unlock(mtx);
}

void delete_node(Node **head, int id, pthread_mutex_t *mtx) {

    Node *delete, *prev, *current;

    pthread_mutex_lock(mtx);
    
    if ((*head) != NULL) {

        if ((*head)->id_serv_cl == id) {
            delete = (*head);
            (*head) = (*head)->next;
            close(delete->fd);
            free(delete);
        } else {
            prev = (*head);
            current = (*head)->next;
            while (current != NULL && current->id_serv_cl != id) {
                prev = current;
                current = current->next;
            }
            if (current != NULL) {
                prev->next = current->next;
                close(current->fd);
                free(current);
            }
        }
    }
    ids[id] = -1;
    
    pthread_mutex_unlock(mtx);
}

Node *get_IDnode(int id, Node **head, pthread_mutex_t *mtx) {
    
    Node *temp;

    if ((*head) != NULL) {
        pthread_mutex_lock(mtx);
        temp = (*head);

        while(temp != NULL) {
            if (temp->id_serv_cl == id) {
                pthread_mutex_unlock(mtx);
                return temp;
            }
            
            temp = temp->next;
        }
        pthread_mutex_unlock(mtx);
    }

    return NULL;
}

void *processing_cls(void *id) {

    struct response resp;
    struct timespec begin, end, timestamp, ts;
    int fd_output, i;
    int *p_id = (int*)id;

    /*Tiempo aleatorio del sleep*/
    ts.tv_sec = 0;
    ts.tv_nsec = (rand () % (76) + 75) * 1000000;
    
    /*String para escribir el contador del fichero*/
    char str_counter[MAX_TXT];

    /*Nodo escritor o lector*/
    Node *node_w = get_IDnode(*p_id, &head_cl, &mtx_list);

    pthread_mutex_lock(&mtx_cl_count);
    n_cl++;  // Revisar por si se puede poner en el main y disminuirlo en cada hilo lector o escritor
    pthread_mutex_unlock(&mtx_cl_count);

    if (clock_gettime(CLOCK_MONOTONIC, &begin) < 0) {
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    /*Procesamiento de escritor y lector*/
    if (node_w->rq.action == WRITE) {

        pthread_mutex_lock(&mtx_wrtrs);
        n_wrtrs++;

        /*Caso con hambruna*/
        /*Se bloquean mientras que haya lectores y escritores no tengan la prioridad*/
        if (params.ratio == -2) {
            while (priority_rdrs && n_rdrs > 0) {
                pthread_cond_wait(&cond_wrtrs, &mtx_wrtrs);
            }
        } else {
            if (!priority_rdrs && n_rdrs > 0) {
                counter_writers++;
                if (counter_writers > params.ratio) {
                    pthread_cond_wait(&cond_wrtrs, &mtx_wrtrs);
                }
            } else if (priority_rdrs && n_rdrs > 0) {
                counter_writers++;
                pthread_cond_wait(&cond_wrtrs, &mtx_wrtrs);
            }
        }
        
        /*Inicio de la Región Crítica*/
        if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {
            warnx("clock_gettime failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }

        counter++;
        /*Rellenar los campos de la respuesta*/
        resp.counter = counter;
        resp.waiting_time = (end.tv_sec-begin.tv_sec)*1e9 + (end.tv_nsec-begin.tv_nsec);

        pthread_mutex_lock(&mtx_file);

        /*Escribir en el fichero, si no existe crearlo, si existe truncarlo*/
        fd_output = open("server_output.txt", O_RDWR|O_CREAT|O_TRUNC, 0660);
        snprintf(str_counter, MAX_TXT, "%d", counter);
        if (write(fd_output, str_counter, strlen(str_counter)) != strlen(str_counter)) {
            warnx("write failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }
        close(fd_output);

        pthread_mutex_unlock(&mtx_file);

        /*Impresión de la traza del Escritor con su timestamp*/
        if (clock_gettime(CLOCK_MONOTONIC, &timestamp) < 0) {
            warnx("clock_gettime failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }

        printf("[%ld.%09ld][ESCRITOR #%d] modifica con valor %d\n",
                timestamp.tv_sec, timestamp.tv_nsec, node_w->rq.id, counter);

        /*Sleep en función de un tiempo aleatorio y envió del mensaje*/
        nanosleep(&ts, NULL);
        send_response(node_w->fd, resp);
        /*Fin de la Región Crítica*/

        /*Escritor ha acabado, se reduce el contador de escritores
        y se procede a liberar a otro escritor/lector.*/
        pthread_mutex_lock(&mtx_cl_count);
        n_cl--;
        pthread_mutex_unlock(&mtx_cl_count);
        n_wrtrs--;

        if (!priority_rdrs && params.ratio != -2 && n_rdrs > 0) {
            finished_writers++;
            if (finished_writers == params.ratio && counter_writers > 0) {
                pthread_mutex_lock(&mtx_rdrs);
                pthread_cond_signal(&cond_rdrs);
                pthread_mutex_unlock(&mtx_rdrs);
            }
        }
        
        pthread_mutex_unlock(&mtx_wrtrs);

        if (params.ratio == -2) {
            if (n_wrtrs > 0) {
                pthread_mutex_lock(&mtx_wrtrs);
                pthread_cond_signal(&cond_wrtrs);
                pthread_mutex_unlock(&mtx_wrtrs);
            } else if (n_rdrs >= n_cl) {
                pthread_mutex_lock(&mtx_rdrs);
                pthread_cond_signal(&cond_rdrs);
                pthread_mutex_unlock(&mtx_rdrs);
            }
        } else {
            if (priority_rdrs) {
                if (wait_rdrs > 0) {
                    pthread_mutex_lock(&mtx_rdrs);
                    n_pass_lctr = 0;
                    for (i = 0; i < params.ratio; i++) {
                        if (pthread_cond_signal(&cond_rdrs) != 0) {
                            i--;
                        }
                    }
                    pthread_mutex_unlock(&mtx_rdrs);
                } else {
                    pthread_mutex_lock(&mtx_wrtrs);
                    pthread_cond_signal(&cond_wrtrs);
                    pthread_mutex_unlock(&mtx_wrtrs);
                }
            } else {
                if (finished_writers == params.ratio) {
                    pthread_mutex_lock(&mtx_wrtrs);
                    finished_writers = 0;
                    pthread_mutex_unlock(&mtx_wrtrs);
                } else if (n_rdrs >= n_cl) {
                    pthread_mutex_lock(&mtx_rdrs);
                    pthread_cond_broadcast(&cond_rdrs);
                    pthread_mutex_unlock(&mtx_rdrs);

                    pthread_mutex_lock(&mtx_wrtrs);
                    finished_writers = 0;
                    counter_writers = 0;
                    pthread_mutex_unlock(&mtx_wrtrs);
                } else if (counter_writers > 0) {
                    pthread_mutex_lock(&mtx_wrtrs);
                    pthread_cond_signal(&cond_wrtrs);
                    pthread_mutex_unlock(&mtx_wrtrs);
                }
            }   
        }
    } else {

        pthread_mutex_lock(&mtx_rdrs);
        n_rdrs++;

        if (params.ratio == -2) {
            while (!priority_rdrs && n_wrtrs > 0) {
                pthread_cond_wait(&cond_rdrs, &mtx_rdrs);
            }
        } else {
            if (!priority_rdrs && n_wrtrs > 0) {
                pthread_cond_wait(&cond_rdrs, &mtx_rdrs);
            } else if ((priority_rdrs && n_wrtrs > 0) || sin_hambruna_rdrs) {
                sin_hambruna_rdrs = 1; // Se va a acceder siempre aqui ya que han aparecido escritores
                node_w->free_cl = 1; // Se marcan los lectores que han detectado escritores para tenerlos en cuenta en el ratio
                
                while (counter_readers >= params.ratio) {
                    wait_rdrs++;
                    pthread_cond_wait(&cond_rdrs, &mtx_rdrs);
                    wait_rdrs--;
                    n_pass_lctr++;
                    if (n_pass_lctr <= params.ratio) {
                        break;
                    }
                }

                counter_readers++;
            }
        }
        pthread_mutex_unlock(&mtx_rdrs);

        /*Inicio de la Región Crítica*/
        if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {
            warnx("clock_gettime failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }

        /*Rellenar los campos de la respuesta*/
        resp.counter = counter;
        resp.waiting_time = (end.tv_sec-begin.tv_sec)*1e9 + (end.tv_nsec-begin.tv_nsec);

        /*Impresión de la traza del Lector con su timestamp*/
        if (clock_gettime(CLOCK_MONOTONIC, &timestamp) < 0) {
            warnx("clock_gettime failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }

        printf("[%ld.%09ld][LECTOR #%d] lee contador con valor %d\n",
                timestamp.tv_sec, timestamp.tv_nsec, node_w->rq.id, resp.counter);

        /*Sleep en función de un tiempo aleatorio y envió del mensaje*/
        nanosleep(&ts, NULL);
        send_response(node_w->fd, resp);
        /*Fin de la Región Crítica*/

        pthread_mutex_lock(&mtx_rdrs);
        n_rdrs--;

        if (node_w->free_cl == 1) {
            rdrs_finished++;
        }
        
        pthread_mutex_lock(&mtx_cl_count);
        n_cl--;
        pthread_mutex_unlock(&mtx_cl_count);

        if (priority_rdrs && params.ratio != -2 && rdrs_finished > 0) {
            if (node_w->free_cl == 1 && rdrs_finished == params.ratio) {
                rdrs_finished = 0;
                pthread_mutex_lock(&mtx_wrtrs);
                pthread_cond_signal(&cond_wrtrs);
                pthread_mutex_unlock(&mtx_wrtrs);
            }

            if (node_w->free_cl == 1 && wait_rdrs == 0 && n_cl >= n_wrtrs) {
                counter_readers = 0;
                sin_hambruna_rdrs = 0;
                rdrs_finished = 0;
                pthread_mutex_lock(&mtx_wrtrs);
                pthread_cond_signal(&cond_wrtrs);
                pthread_mutex_unlock(&mtx_wrtrs);
            }
        }
        pthread_mutex_unlock(&mtx_rdrs);

        if (params.ratio == -2) {
            if (n_rdrs > 0) {
                pthread_mutex_lock(&mtx_rdrs);
                pthread_cond_broadcast(&cond_rdrs);
                pthread_mutex_unlock(&mtx_rdrs);
            } else if (n_wrtrs >= n_cl) {
                pthread_mutex_lock(&mtx_wrtrs);
                pthread_cond_signal(&cond_wrtrs);
                pthread_mutex_unlock(&mtx_wrtrs);
            }
        } else {
            if (!priority_rdrs) {
                if (n_wrtrs > 0) {
                    pthread_mutex_lock(&mtx_wrtrs);
                    pthread_cond_signal(&cond_wrtrs);
                    pthread_mutex_unlock(&mtx_wrtrs);
                } else {
                    pthread_mutex_lock(&mtx_rdrs);
                    pthread_cond_broadcast(&cond_rdrs);
                    pthread_mutex_unlock(&mtx_rdrs);
                }
            }
        }
    }
    
    /*Eliminación del nodo y señal en semáforo de que un cliente ha terminado*/
    delete_node(&head_cl, *p_id, &mtx_list);
    sem_post(&sem);

    pthread_exit(NULL);
}

void handler_off_server() {

    int i;

    pthread_mutex_destroy(&mtx_wrtrs);
    pthread_mutex_destroy(&mtx_rdrs);
    pthread_mutex_destroy(&mtx_cl_count);
    pthread_mutex_destroy(&mtx_file);
    pthread_cond_destroy(&cond_rdrs);
    pthread_cond_destroy(&cond_wrtrs);
    sem_destroy(&sem);

    for(i = 0; i < ind; i++) {
        delete_node(&head_cl, i, &mtx_list);
    }

    pthread_mutex_destroy(&mtx_list);

    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {

    pthread_t thrds_cls[MAX_CLTS];
    int fd_output;
    char str_counter[MAX_TXT];

    sem_init(&sem, 0, MAX_CLTS); // Controla el número máximo de threads creados
    pthread_mutex_init(&mtx_list, NULL); // Candado para proteger la lista de nodos
    pthread_mutex_init(&mtx_file, NULL);
    pthread_mutex_init(&mtx_wrtrs, NULL);
    pthread_mutex_init(&mtx_rdrs, NULL);
    pthread_mutex_init(&mtx_cl_count, NULL);

    pthread_cond_init(&cond_rdrs, NULL);
    pthread_cond_init(&cond_wrtrs, NULL);

    srand(time(NULL));
    setbuf(stdout, NULL);

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: --port PORT --priority writer/reader");
    }

    save_params(argc, argv, &params);

    if (argc-1 == 4 || (argc-1 == 6 && params.ratio == 0)) {
        params.ratio = -2;
    }

    if (init_serv("0.0.0.0", params.port) < 0) {
        exit(EXIT_FAILURE);
    }

    if (access("server_output.txt", F_OK) != -1) {
        fd_output = open("server_output.txt", O_RDONLY);
        if (read(fd_output, str_counter, MAX_TXT) < 0) {
            err(EXIT_FAILURE, "read failure");
        }
        counter = atoi(str_counter);
        close(fd_output);
    }

    signal(SIGINT, handler_off_server);
    
    while (1) {
        struct request req;
        int cl_fd;

        // Espero cliente
        wait_cl(&cl_fd);
        sem_wait(&sem);
        receiving_rqs(cl_fd, &req);

        add_cl(&head_cl, cl_fd, req, &mtx_list); // Añado cliente a la lista de nodos
        
        if (pthread_create(&thrds_cls[ind], NULL, processing_cls, &ids[ind]) != 0) {
            sem_destroy(&sem);
            warnx("error creating thread");
            exit(EXIT_FAILURE);
        }
    }
    
    exit(EXIT_SUCCESS);
}