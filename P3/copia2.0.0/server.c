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
#include <err.h>

#define MAX_CLTS 250
#define MAX_TXT 1024

struct Node
{
    int id;
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
int ids[MAX_CLTS]; // Para filtrar
int ids_readers[MAX_CLTS]; // Para filtrar
int counter = 0;
int ind = 0, ind_r = 0, ind_w = 0, id_r = 0, id_w = 0;
int fd_output;

int priority_rdrs = 1; // // Indica si es el turno de los lectores (1) o de los escritores (0) Prioridad

int finished_writers = 0, finished_readers = 0;
int n_wrtrs = 0;
int n_rdrs = 0;

int n_wrtrs_waiting = 0; // Número de escritores en cola esperando su turno
int n_rdrs_waiting = 0;

pthread_mutex_t mtx_wrtrs;
pthread_mutex_t mtx_rdrs;
pthread_mutex_t mtx_cl_count;
pthread_mutex_t mtx_list;
pthread_cond_t cond_rdrs;
pthread_cond_t cond_wrtrs;
sem_t sem;

Node *head_rdrs = NULL;
Node *head_wrtrs = NULL;

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


void add_cl(Node **head, int id, int fd, pthread_mutex_t *mtx) {

    // Crear el nuevo nodo
    Node* last;
    Node* new = (Node*) malloc(sizeof(Node));
    new->id = id;
    new->fd = fd;
    new->next = NULL;
    
    // Adquirir el mutex de lectores
    pthread_mutex_lock(mtx);
    
    // Añadir el nuevo nodo al final de la lista
    if ((*head) == NULL) {
        (*head) = new;
    } else {
        last = (*head);
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = new;
    }
    
    // Liberar el mutex de lectores
    pthread_mutex_unlock(mtx);
}

void delete_node(Node **head, int id, pthread_mutex_t *mtx) {

    Node *delete, *prev, *current;

    // Adquirir el mutex de lectores
    pthread_mutex_lock(mtx);
    
    // Eliminar el nodo de la lista
    if ((*head) != NULL) {

        if ((*head)->id == id) {
            delete = (*head);
            (*head) = (*head)->next;
            close(delete->fd);
            free(delete);
        } else {
            prev = (*head);
            current = (*head)->next;
            while (current != NULL && current->id != id) {
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
    
    // Liberar el mutex de lectores
    pthread_mutex_unlock(mtx);
}

Node *get_IDnode(int id, Node **head, pthread_mutex_t *mtx) {
    
    Node *temp;

    if ((*head) != NULL) {
        pthread_mutex_lock(mtx);
        temp = (*head);

        while(temp != NULL) {
            if (temp->id == id) {
                pthread_mutex_unlock(mtx);
                return temp;
            }
            
            temp = temp->next;
        }
        pthread_mutex_unlock(mtx);
    }

    return NULL;
}
int free_rd = 0;
int wait_hungry = 0;

/*void *processing_wrtrs(void *id) {

    struct response resp;
    struct timespec begin,end,timestamp;
    int *p_id = (int*)id;
    int time_sleep = rand () % (76) + 75;   // Este está entre 75 y 150
    char str_counter[MAX_TXT];

    Node *node_w = get_IDnode(*p_id, &head_wrtrs, &mtx_wrtrs);
    resp.action = WRITE;

    pthread_mutex_lock(&mtx_wrtrs);
    n_wrtrs++;
    pthread_mutex_unlock(&mtx_wrtrs);

    if (clock_gettime(CLOCK_MONOTONIC, &begin) < 0) {   //  take the initial time
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mtx_priority);

    // Solo entra si no tiene prioridad
    while (n_rdrs > 0 && priority_rdrs) {
        printf("SOY ESCRITOR Y ENTRO A ESPERAR\n");
        pthread_cond_wait(&cond_wrtrs, &mtx_priority);
    }


    while (n_rdrs > 0 && !priority_rdrs && params.ratio != -2 && finished_writers != 0 && finished_writers % params.ratio == 0) {//finished_writers == params.ratio) { // solo accede a esta parte si existe ratio(!=-2)
        printf("#SOY ESCRITOR %d Y ESPERO PARA DEJAR PASO A UN LECTOR, %d#\n", *p_id, !free_rd);
        if (!free_rd) {
            printf("#LIBERO LECTOR#\n");
            pthread_cond_signal(&cond_rdrs); // solo se lo quiero enviar a un lector
            free_rd = 1;
        }
        // se van acumulando, necesito que salgan solo los que indique ratio
        pthread_cond_wait(&cond_wrtrs, &mtx_priority);
        printf("#YA DEJE PASAR A UN LECTOR Y TERMINO, AHORA CONTINUO YO#\n");
        if (!free_rd) {
            break;
        }
    }
    
    //if (finished_writers != params.ratio) {
    finished_writers++;
    //}

    // Region critica

    ////////////////////
    if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {   //  take the initial time
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    // Modificacion de contador y envio de informacion
    counter++;
    resp.counter = counter;
    resp.waiting_time = (end.tv_sec-begin.tv_sec)*1e9 + (end.tv_nsec-begin.tv_nsec);
    
    fd_output = open("server_output.txt", O_RDWR|O_CREAT|O_TRUNC, 0660); // Quiza ponerlo con variable local
    snprintf(str_counter, MAX_TXT, "%d", counter);
    if (write(fd_output, str_counter, strlen(str_counter)) != strlen(str_counter)) {
        warnx("write failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }
    close(fd_output);

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) < 0) {   //  take the timestamp
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }
    printf("[%ld.%ld][ESCRITOR #%d] modifica con valor %d\n",
            timestamp.tv_sec, timestamp.tv_nsec, node_w->id, counter);

    usleep(time_sleep);
    send_response(node_w->fd, resp);
    ////////////////////

    ////////////////////
    
    if (params.ratio != -2 && n_rdrs > 0) {
        int i;
        printf("LIBERO %d ESCRITORES, NO TENGO PRIORIDAD\n", params.ratio);
        for (i = 0; i < params.ratio; i++) {
            pthread_cond_signal(&cond_wrtrs);
        }
    } else {
        pthread_cond_signal(&cond_wrtrs);
    }
    
    pthread_mutex_unlock(&mtx_priority);

    


    pthread_mutex_lock(&mtx_wrtrs);
    n_wrtrs--;
    pthread_mutex_unlock(&mtx_wrtrs);


    pthread_mutex_lock(&mtx_priority);
    
    if (n_wrtrs == 0 && n_rdrs > 0) {
        pthread_cond_broadcast(&cond_rdrs);
    }
    
    
    pthread_mutex_unlock(&mtx_priority);

    
    delete_node(&head_wrtrs, *p_id, &mtx_wrtrs);
    sem_post(&sem);

    pthread_exit(NULL);
}

void *processing_rdrs(void *id) {

    struct response resp;
    struct timespec begin,end,timestamp;
    int *p_id = id;
    int time_sleep = rand () % (76) + 75;   // Este está entre 75 y 150

    Node *node_r = get_IDnode(*p_id, &head_rdrs, &mtx_rdrs);

    resp.action = READ;

    if (clock_gettime(CLOCK_MONOTONIC, &begin) < 0) {   //  take the final time
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mtx_rdrs);
    n_rdrs++;
    pthread_mutex_unlock(&mtx_rdrs);


    pthread_mutex_lock(&mtx_priority);

    // Si hay algún hilo escritor en curso y la prioridad no está en lectores, esperar a que finalice
    while (n_wrtrs > 0 && !priority_rdrs) {
        printf("SOY LECTOR %d Y ENTRO A ESPERAR\n", *p_id);
        pthread_cond_wait(&cond_rdrs, &mtx_priority);
        printf("ESCRITORES: %d\n", finished_writers);
        if (free_rd) {
            //free_rd = 0;
            printf("LECTOR %d es liberado\n", *p_id);
            break;
        }
    }
    printf("SOY LECTOR %d Y TERMINE DE ESPERAR\n", *p_id);


    if (finished_readers-1 != params.ratio) {
        finished_readers++;
    }

    if (n_wrtrs > 0 && priority_rdrs && finished_readers-1 == params.ratio) {
        printf("SOY LECTOR Y ESPERO PARA DEJAR PASO A UN ESCRITOR\n");
        //finished_readers = 0;
        pthread_cond_signal(&cond_wrtrs);

        pthread_cond_wait(&cond_rdrs, &mtx_priority);
    }


    // Region critica
    ////////////////////
    if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {   //  take the final time
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    resp.counter = counter;
    resp.waiting_time = (end.tv_sec-begin.tv_sec)*1e9 + (end.tv_nsec-begin.tv_nsec);

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) < 0) {   //  take the timestamp
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }
    printf("[%ld.%ld][LECTOR #%d] lee contador con valor %d\n",
            timestamp.tv_sec, timestamp.tv_nsec, node_r->id, resp.counter);
    
    usleep(time_sleep);
    send_response(node_r->fd, resp);
    ////////////////////

    if (params.ratio != -2 && n_wrtrs > 0) {
        int i;
        printf("LIBERO %d ESCRITORES, NO TENGO PRIORIDAD\n", params.ratio);
        for (i = 0; i < params.ratio; i++) {
            pthread_cond_signal(&cond_wrtrs);
        }
    } else {
        pthread_cond_signal(&cond_wrtrs);
    }
    
    pthread_mutex_unlock(&mtx_priority);

    
    

    pthread_mutex_lock(&mtx_rdrs);
    n_rdrs--;
    pthread_mutex_unlock(&mtx_rdrs);


    pthread_mutex_lock(&mtx_priority);
    
    if (n_rdrs == 0 && n_wrtrs > 0) {
        pthread_cond_broadcast(&cond_wrtrs);
    }
    
    pthread_mutex_unlock(&mtx_priority);



    delete_node(&head_rdrs, *p_id, &mtx_rdrs);
    sem_post(&sem);

    pthread_exit(NULL);
}*/

void* processing_rdrs(void* id) {

    // Recuperar los parámetros
    struct response resp;
    struct timespec begin,end,timestamp;
    int *p_id = id;
    int time_sleep = rand () % (76) + 75;   // Este está entre 75 y 150

    Node *node_r = get_IDnode(*p_id, &head_rdrs, &mtx_rdrs);

    resp.action = READ;

    if (clock_gettime(CLOCK_MONOTONIC, &begin) < 0) {   //  take the final time
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    
    // Adquirir el mutex de lectores
    pthread_mutex_lock(&mtx_rdrs);

    n_rdrs_waiting++;
    printf("NUMEROS DE ESCRITORES EN LECTOR %d: %d %d\n", *p_id, n_wrtrs, n_wrtrs_waiting);
    // Si hay escritores en curso o es el turno de los escritores, esperar en la variable de condición de lectores
    if (!priority_rdrs && (n_wrtrs > 0 || n_wrtrs_waiting > 0)) {
        printf("LECTOR %d A ESPERAR\n", *p_id);
        pthread_cond_wait(&cond_rdrs, &mtx_rdrs);
        printf("LECTOR %d LIBERADO\n", *p_id);
    }

    // Decrementar e incrementar el número de lectores en cola y en curso
    n_rdrs_waiting--;
    n_rdrs++;

    // Liberar el mutex de lectores
    pthread_mutex_unlock(&mtx_rdrs);

    
    pthread_mutex_lock(&mtx_cl_count);
    // Leer del archivo
    // Region critica
    ////////////////////
    if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {   //  take the final time
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    resp.counter = counter;
    resp.waiting_time = (end.tv_sec-begin.tv_sec)*1e9 + (end.tv_nsec-begin.tv_nsec);

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) < 0) {   //  take the timestamp
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }
    printf("[%ld.%ld][LECTOR #%d] lee contador con valor %d\n",
            timestamp.tv_sec, timestamp.tv_nsec, node_r->id, resp.counter);
    
    usleep(time_sleep);
    send_response(node_r->fd, resp);
    ////////////////////
    
    pthread_mutex_unlock(&mtx_cl_count);
    
    // Adquirir el mutex de lectores
    pthread_mutex_lock(&mtx_rdrs);
    
    // Decrementar el número de lectores en curso y cambiar el turno a escritores
    n_rdrs--;
    finished_readers++;

    if (strcmp(params.priority, "writer") == 0) {
        priority_rdrs = 0;
    } else {
        priority_rdrs = 1;
    }
    
    
    // Si hay escritores en cola, señalizar la variable de condición de escritores
    /*if (n_wrtrs_waiting > 0) {
        pthread_cond_signal(&cond_wrtrs);
    }*/

    if ((n_wrtrs > 0 || n_wrtrs_waiting > 0) && !priority_rdrs) {
        // Si es el turno de los escritores, señalizar la variable de condición de escritores
        for (int i = 0; i < params.ratio; i++) {
            pthread_cond_signal(&cond_wrtrs);
            //finished_writers--;    
        }
            
        printf("LECTOR %d ENVIA SEÑAL A ESCRITOR, TIENE PRIORIDAD\n", *p_id);
    }
    // Si no es el turno de los escritores y hay tres o más lectores en cola, cambiar el turno a escritores
    else if (params.ratio != -2 && finished_readers % params.ratio == 0 && strcmp(params.priority, "reader") == 0) {
        printf("NO ME METO POR AHORA\n");
        priority_rdrs = 0;
        pthread_cond_signal(&cond_rdrs);
    }
    
    // Liberar el mutex de lectores
    pthread_mutex_unlock(&mtx_rdrs);

    delete_node(&head_rdrs, *p_id, &mtx_rdrs);
    sem_post(&sem);

    pthread_exit(NULL);
}
int let_reader = 0;

void* processing_wrtrs(void* id) {

    // Recuperar los parámetros
    struct response resp;
    struct timespec begin,end,timestamp;
    int *p_id = (int*)id;
    int time_sleep = rand () % (76) + 75;   // Este está entre 75 y 150
    char str_counter[MAX_TXT];

    Node *node_w = get_IDnode(*p_id, &head_wrtrs, &mtx_wrtrs);
    resp.action = WRITE;
    
    if (clock_gettime(CLOCK_MONOTONIC, &begin) < 0) {   //  take the initial time
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    // Adquirir el mutex de escritores
    pthread_mutex_lock(&mtx_wrtrs);
    
    // Incrementar el número de escritores en cola
    n_wrtrs_waiting++;

    if (params.ratio != -2 && n_wrtrs != 0 && n_wrtrs % params.ratio == 0) {
        printf("ESCRITOR %d DEJA PASO A LECTOR\n", *p_id);
        let_reader = 1;
        priority_rdrs = 1;
        if (!free_rd) {
            pthread_cond_signal(&cond_rdrs);
            free_rd = 1;
        }
    }

    
    
    // Si hay lectores en curso y es el turno de los lectores,
    // esperar en la variable de condición de escritores
    if (((n_rdrs > 0 || n_rdrs_waiting > 0) && priority_rdrs) || let_reader) {
        printf("ESCRITOR %d ESPERANDO\n", *p_id);
        pthread_cond_wait(&cond_wrtrs, &mtx_wrtrs);
        let_reader = 0;
        free_rd = 0;
        printf("ESCRITOR %d LIBERADO\n", *p_id);
        //break;
    }
    printf("ESCRITOR %d NO LE AFECTA EL CAMBIO DE PRIORIDAD\n", *p_id);

    // Decrementar el número de escritores en cola y aumentar el número de escritores en curso
    n_wrtrs_waiting--;
    n_wrtrs++;

    // Liberar el mutex de escritores
    pthread_mutex_unlock(&mtx_wrtrs);
    

    pthread_mutex_lock(&mtx_cl_count);
    // Escribir en el archivo
    // Region critica

    ////////////////////
    if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {   //  take the initial time
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    // Modificacion de contador y envio de informacion
    counter++;
    resp.counter = counter;
    resp.waiting_time = (end.tv_sec-begin.tv_sec)*1e9 + (end.tv_nsec-begin.tv_nsec);
    
    fd_output = open("server_output.txt", O_RDWR|O_CREAT|O_TRUNC, 0660); // Quiza ponerlo con variable local
    snprintf(str_counter, MAX_TXT, "%d", counter);
    if (write(fd_output, str_counter, strlen(str_counter)) != strlen(str_counter)) {
        warnx("write failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }
    close(fd_output);

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) < 0) {   //  take the timestamp
        warnx("clock_gettime failed");
        sem_post(&sem);
        pthread_exit(NULL);
    }
    printf("[%ld.%ld][ESCRITOR #%d] modifica con valor %d\n",
            timestamp.tv_sec, timestamp.tv_nsec, node_w->id, counter);

    usleep(time_sleep);
    send_response(node_w->fd, resp);
    ////////////////////

    ////////////////////

    pthread_mutex_unlock(&mtx_cl_count);

    
    // Adquirir el mutex de escritores
    pthread_mutex_lock(&mtx_wrtrs);
    
    // Decrementar el número de escritores en curso y comprobar si es el turno de los lectores
    n_wrtrs--;
    finished_writers++;

    if (strcmp(params.priority, "reader") == 0) {
        priority_rdrs = 1;
    } else {
        priority_rdrs = 0;
    }

    /*if (n_rdrs_waiting > 0) {
        pthread_cond_signal(&cond_rdrs);
    }*/
    
    printf("NUMERO DE ESCRITORES: %d\n", finished_writers);

    if ((n_rdrs > 0 || n_rdrs_waiting > 0) && priority_rdrs) {
        // Si es el turno de los lectores, señalizar la variable de condición de lectores
        pthread_cond_signal(&cond_rdrs);
    }
    // Si no es el turno de los lectores y hay tres o más escritores en cola, cambiar el turno a lectores
    else if (params.ratio != -2 && finished_writers-1 % params.ratio == 0 && strcmp(params.priority, "writer") == 0) {
        //printf("ESCRITOR %d DEJA PASO A LECTOR\n", *p_id);
        //priority_rdrs = 1;
        //pthread_cond_signal(&cond_rdrs);
    }

    if (n_wrtrs_waiting == 0 && n_wrtrs == 0 && (n_rdrs_waiting > 0 || n_rdrs > 0)) {
        pthread_cond_broadcast(&cond_rdrs);
    }
    
    // Liberar el mutex de escritores
    pthread_mutex_unlock(&mtx_wrtrs);

    delete_node(&head_wrtrs, *p_id, &mtx_wrtrs);
    sem_post(&sem);

    pthread_exit(NULL);
}



int main(int argc, char *argv[]) {

    pthread_t thr_rd[MAX_CLTS];
    pthread_t thr_wr[MAX_CLTS];
    struct request req;
    char str_counter[MAX_TXT];

    sem_init(&sem, 0, MAX_CLTS); // Se colocan el numero de clientes que quiero que entren en la region critica
    pthread_mutex_init(&mtx_wrtrs, NULL);
    pthread_mutex_init(&mtx_rdrs, NULL);
    pthread_mutex_init(&mtx_cl_count, NULL);
    pthread_mutex_init(&mtx_list, NULL);
    pthread_cond_init(&cond_rdrs, NULL);
    pthread_cond_init(&cond_wrtrs, NULL);
    srand(time(NULL));

    setbuf(stdout, NULL);

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: --port PORT --priority writer/reader");
    }

    save_params(argc, argv, &params);

    if (argc == 4) {
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
    int cl_fd;
    while (1) {

        wait_cl(&cl_fd); // Espero clientes
        sem_wait(&sem);
        receiving_rqs(cl_fd, &req);

        // Recibo mensajes de clientes y hay que filtrarlos segun escritor y lector
        if (req.action == WRITE) {
            //id_p = find_slot(ids_pubs, MAX_PUBS);
            ids[ind_w] = req.id;
            add_cl(&head_wrtrs, ids[ind_w], cl_fd, &mtx_wrtrs);
            
            if (pthread_create(&thr_wr[ind_w], NULL, processing_wrtrs, &ids[ind_w]) != 0) {
                sem_destroy(&sem);
                //pthread_mutex_destroy(&mtx);
                close(fd_output);
                warnx("error creating thread");
                exit(EXIT_FAILURE);
            }
            
            ind_w++;
            if (ind_w >= MAX_CLTS) {
                ind_w = 0;
            }
        } else {
            ids_readers[ind_r] = req.id;
            add_cl(&head_rdrs, ids_readers[ind_r], cl_fd, &mtx_rdrs);
            if (pthread_create(&thr_rd[ind_r], NULL, processing_rdrs, &ids_readers[ind_r]) != 0) {
                sem_destroy(&sem);
                //pthread_mutex_destroy(&mtx);
                close(fd_output);
                warnx("error creating thread");
                exit(EXIT_FAILURE);
            }
            ind_r++;
            if (ind_r >= MAX_CLTS) {
                ind_r = 0;
            }
        }
        ind++;

        if (ind >= MAX_CLTS) {
            ind = 0;
        }
    }
    
    exit(EXIT_SUCCESS);
}