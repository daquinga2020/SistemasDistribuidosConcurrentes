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

int fds[MAX_CLTS];
int ids[MAX_CLTS];
int counter = 0;
int ind = 0, ind_r = 0, ind_w = 0, id_r = 0, id_w = 0;
int fd_output;

int priority_rdrs = 1; // // Indica si es prioritario atender a los lectores (1) o a los escritores (0)

int finished_writers = 0, finished_readers = 0;
int n_wrtrs = 0, n_rdrs = 0;

int n_wrtrs_waiting = 0, n_rdrs_waiting = 0;

int first_wr = 0, first_rd = 0;
int n_cl = 0;

pthread_mutex_t mtx_wrtrs;
pthread_mutex_t mtx_rdrs;
pthread_mutex_t mtx_cl_count;
pthread_mutex_t mtx_list;
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


void add_cl(Node **head, int fd, int id, struct request rq, pthread_mutex_t *mtx) {

    Node* last;
    Node* new = (Node*) malloc(sizeof(Node));
    new->rq = rq;
    new->fd = fd;
    new->id_serv_cl = id;
    new->next = NULL;
    
    pthread_mutex_lock(mtx);
    
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
    struct timespec begin, end, timestamp;
    int *p_id = (int*)id;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = (rand () % (76) + 75) * 1000000;
    char str_counter[MAX_TXT];

    Node *node_w = get_IDnode(*p_id, &head_cl, &mtx_list);

    pthread_mutex_lock(&mtx_cl_count);
    n_cl++;
    pthread_mutex_unlock(&mtx_cl_count);

    if (clock_gettime(CLOCK_MONOTONIC, &begin) < 0) {
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    if (node_w->rq.action == WRITE) {
        resp.action = WRITE;

        pthread_mutex_lock(&mtx_wrtrs);
        n_wrtrs_waiting++;
        
        if (first_wr && n_rdrs_waiting > 0) {
            if (params.ratio != -2 || (priority_rdrs && n_rdrs_waiting > 0 && params.ratio == -2)) {
                pthread_cond_wait(&cond_wrtrs, &mtx_wrtrs);
            }
        }
        first_wr = 1;

        if (!first_wr && n_wrtrs == 0 && priority_rdrs && n_rdrs_waiting > 0) {
            pthread_cond_wait(&cond_wrtrs, &mtx_wrtrs);
        }
        
        n_wrtrs++;
        if (n_rdrs_waiting > 0) {
            finished_writers++;
        }
        
        if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {
            warnx("clock_gettime failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }

        // Modificacion de contador y envio de informacion
        counter++;
        resp.counter = counter;
        resp.waiting_time = (end.tv_sec-begin.tv_sec)*1e9 + (end.tv_nsec-begin.tv_nsec);
        
        fd_output = open("server_output.txt", O_RDWR|O_CREAT|O_TRUNC, 0660);
        snprintf(str_counter, MAX_TXT, "%d", counter);
        if (write(fd_output, str_counter, strlen(str_counter)) != strlen(str_counter)) {
            warnx("write failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }
        close(fd_output);

        if (clock_gettime(CLOCK_MONOTONIC, &timestamp) < 0) {
            warnx("clock_gettime failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }
        printf("[%ld.%09ld][ESCRITOR #%d] modifica con valor %d\n",
                timestamp.tv_sec, timestamp.tv_nsec, node_w->rq.id, counter);

        nanosleep(&ts, NULL);
        send_response(node_w->fd, resp);

        n_wrtrs_waiting--;

        if (params.ratio != -2) {

            // Cuento si se ha cumplido el ratio para liberar a escritor o lector
            if ((!priority_rdrs && finished_writers != 0 && finished_writers % params.ratio == 0 && n_rdrs_waiting > 0) || (priority_rdrs && n_rdrs_waiting > 0)) {
                pthread_cond_signal(&cond_rdrs);
            } else if (!priority_rdrs || (priority_rdrs && n_rdrs_waiting == 0)){
                pthread_cond_signal(&cond_wrtrs);
                if (!priority_rdrs && n_wrtrs_waiting == 0 && n_rdrs_waiting > 0) {
                    pthread_cond_signal(&cond_rdrs);
                } else {
                    pthread_cond_signal(&cond_wrtrs);
                }
            }

        } else {
            if (priority_rdrs && n_rdrs_waiting > 0) {
                pthread_cond_signal(&cond_rdrs);
            } else {
                if (n_cl - n_rdrs - n_wrtrs - n_rdrs_waiting == 0) {
                    pthread_cond_signal(&cond_rdrs);
                } else {
                    pthread_cond_signal(&cond_wrtrs);
                }
                
            }
        }
        
        pthread_mutex_unlock(&mtx_wrtrs);
    } else {

        resp.action = READ;

        pthread_mutex_lock(&mtx_rdrs);
    
        n_rdrs_waiting++;

        pthread_mutex_unlock(&mtx_rdrs);

        // Entran todos menos el primero.
        if (first_rd && n_wrtrs_waiting > 0) {
            // Espero si hay un ratio o si la prioridad es de escritores y hay escritores esperando y hay un ratio
            /*if (params.ratio != -2 || (!priority_rdrs && n_wrtrs_waiting > 0 && params.ratio == -2)) {
                pthread_cond_wait(&cond_rdrs, &mtx_rdrs);
            }*/
        }
        
        pthread_mutex_lock(&mtx_rdrs);
        first_rd = 1; // Inicialmente esta a 0.
        pthread_mutex_unlock(&mtx_rdrs);

        // Espero si no soy el primer lector para pasar y no hay lectores y la prioridad es de escritores y hay escritores esperando.
        if (!first_rd && n_rdrs == 0 && !priority_rdrs && n_wrtrs_waiting > 0) {
            // Creo que aqui no entra nunca
            printf("Voy a esperar\n");
            pthread_cond_wait(&cond_rdrs, &mtx_rdrs);
        }

        n_rdrs++;
        if (n_wrtrs_waiting > 0) {
            finished_readers++;
        }

        ///////////////////////////////////////////////
        if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {
            warnx("clock_gettime failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }

        fd_output = open("server_output.txt", O_RDONLY|O_CREAT);
        if (read(fd_output, str_counter, MAX_TXT) < 0) {
            sem_post(&sem);
            perror("read failed");
            pthread_exit(NULL);
        }
        close(fd_output);
        resp.counter = atoi(str_counter);
        resp.waiting_time = (end.tv_sec-begin.tv_sec)*1e9 + (end.tv_nsec-begin.tv_nsec);

        if (clock_gettime(CLOCK_MONOTONIC, &timestamp) < 0) {
            warnx("clock_gettime failed");
            sem_post(&sem);
            pthread_exit(NULL);
        }
        printf("[%ld.%09ld][LECTOR #%d] lee contador con valor %d\n",
                timestamp.tv_sec, timestamp.tv_nsec, node_w->rq.id, resp.counter);
        
        nanosleep(&ts, NULL);
        send_response(node_w->fd, resp);
        ///////////////////////////////////////////////

        n_rdrs_waiting--;

        if (params.ratio != -2) {
            // Cuento si se ha cumplido ratio para liberar a escritor o lector
            if ((priority_rdrs && finished_readers != 0 && finished_readers % params.ratio == 0 && n_wrtrs_waiting > 0) || (!priority_rdrs && n_wrtrs_waiting > 0)) {
                pthread_cond_signal(&cond_wrtrs);

            } else if (priority_rdrs || (!priority_rdrs && n_wrtrs_waiting == 0)){
                
                if (priority_rdrs && n_wrtrs_waiting > 0 && n_rdrs_waiting == 0) {
                    pthread_cond_signal(&cond_wrtrs);
                } else {
                    pthread_cond_signal(&cond_rdrs);
                }
            }

        } else {
            if (!priority_rdrs && n_wrtrs_waiting > 0) {
                pthread_cond_signal(&cond_wrtrs);
            } else {
                
                if (n_cl - n_rdrs - n_wrtrs - n_wrtrs_waiting == 0) {
                    pthread_cond_signal(&cond_wrtrs);
                } else {
                    pthread_cond_signal(&cond_rdrs);
                }
            }
        }

        pthread_mutex_unlock(&mtx_rdrs);
    }

    delete_node(&head_cl, *p_id, &mtx_list);
    sem_post(&sem);

    pthread_exit(NULL);
}

void handler_off_server() {

    int i;

    pthread_mutex_destroy(&mtx_wrtrs);
    pthread_mutex_destroy(&mtx_rdrs);
    pthread_mutex_destroy(&mtx_cl_count);
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
    int id = 0;
    char str_counter[MAX_TXT];

    sem_init(&sem, 0, MAX_CLTS);
    pthread_mutex_init(&mtx_wrtrs, NULL);
    pthread_mutex_init(&mtx_rdrs, NULL);
    pthread_mutex_init(&mtx_list, NULL);
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

        wait_cl(&cl_fd);
        sem_wait(&sem);
        receiving_rqs(cl_fd, &req);

        ids[ind] = id;
        add_cl(&head_cl, cl_fd, ids[ind], req, &mtx_list);

        if (pthread_create(&thrds_cls[ind], NULL, processing_cls, &ids[ind]) != 0) {
            sem_destroy(&sem);
            warnx("error creating thread");
            exit(EXIT_FAILURE);
        }

        id++;
        ind++;

        if (ind >= MAX_CLTS) {
            ind = 0;
        }
    }
    
    exit(EXIT_SUCCESS);
}