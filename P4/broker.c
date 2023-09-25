#include "proxy.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <sys/socket.h>
#include <getopt.h>
#include <semaphore.h>
#include <signal.h>
#include <err.h>

#define MAX_TXT 100
#define MAX_TOPICS 10
#define MAX_PUBS 100 
#define MAX_SUBS 900

struct Params
{
    int port;
    char mode[MAX_TXT];
};
typedef struct Params Params;

struct Data
{
    int id;
    int fd;
    char topic[MAX_TXT];
};
typedef struct Data Data;

typedef struct Node Node;
struct Node
{
    Data data;
    Node *prev;
    Node *next;
};

struct Data2Subs
{
    int n;
    int n_subs_topic;
    int fd;
    struct message msg;
};
typedef struct Data2Subs Data2Subs;

Params params;
Node *head_pub = NULL;
Node *last_pub = NULL;
Node *head_subs = NULL;
Node *last_subs = NULL;
int ids_pubs[MAX_PUBS];
int ids_subs[MAX_SUBS];
int pubs_tps[MAX_TOPICS];
int subs_tps[MAX_TOPICS];
int counter_tp = 0;
int full_pubs = 0;
int full_subs = 0;
int id_p = 0;
int id_s = 0;
int init_subs = 1, init_pub = 1;

char *topics[MAX_TOPICS];

pthread_mutex_t mtx_pubs;
pthread_mutex_t mtx_subs;
pthread_barrier_t barrier;

void createdList(Data data, Node **head, Node **last, pthread_mutex_t *mtx) {

    *head = (Node *)malloc(sizeof(Node));
    pthread_mutex_init(mtx, NULL); // Mutex inicializado

    if (*head == NULL) {
        err(EXIT_FAILURE,"malloc failed");
    }

    (*head)->data = data;
    (*head)->prev = NULL;
    (*head)->next = NULL;

    (*last) = (*head);
}

void insertNdAtEnd(Data data, Node **last, pthread_mutex_t *mtx) {
    
    Node *new_node;

    if((*last) != NULL) {
        // Bloqueamos el acceso a la lista
        pthread_mutex_lock(mtx);
        new_node = (Node *)malloc(sizeof(Node));

        new_node->data = data;
        new_node->next = NULL;
        new_node->prev = (*last);

        (*last)->next = new_node;
        (*last) = new_node;
        // Bloqueamos el acceso a la lista
        pthread_mutex_unlock(mtx);
    }
}

Node *get_IDnode(int id, Node **head, pthread_mutex_t *mtx) {
    
    Node *temp;

    if ((*head) != NULL) {
        pthread_mutex_lock(mtx);
        temp = (*head);

        while(temp != NULL)
        {
            if (temp->data.id == id) {
                pthread_mutex_unlock(mtx);
                return temp;
            }
            
            // Moviendo puntero al siguiente nodo
            temp = temp->next;
        }
        pthread_mutex_unlock(mtx);
    }

    return NULL;
}

void deleteNd_N(int id, Node **head, Node **last, pthread_mutex_t *mtx) {

    Node *current_nd;
    Node *delete_nd;

    current_nd = get_IDnode(id, head, mtx);

    pthread_mutex_lock(mtx);
    if (current_nd != NULL && current_nd == (*head)) {
        close(current_nd->data.fd);
        delete_nd = (*head);
        
        (*head) = (*head)->next;
        
        if ((*head) != NULL) {
            (*head)->prev = NULL;
        }

        free(delete_nd);
    } else if (current_nd != NULL && current_nd == (*last)) {
        close(current_nd->data.fd);
        delete_nd = (*last);

        (*last) = (*last)->prev;
        
        if ((*last) != NULL) {
            (*last)->next = NULL;
        }

        free(delete_nd);
    } else if (current_nd != NULL) {
        close(current_nd->data.fd);

        current_nd->prev->next = current_nd->next;
        current_nd->next->prev = current_nd->prev;

        free(current_nd);
    }
    
    pthread_mutex_unlock(mtx);
}


int check_nargs(int n) {

    return n == 4;
}

void save_params(int n, char **args, Params *p) {
    int c;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"port", required_argument, 0, 'p'},
            {"mode", required_argument, 0, 'm'},
        };

        c = getopt_long (n, args, "p:m:",
                long_options, &option_index);

        if (c == -1)
           break;

        switch (c) {
        case 'p':
            p->port = atoi(optarg);
            break;

        case 'm':
            if (strcmp(optarg, "secuencial") == 0) {
                strncpy(p->mode, optarg, sizeof(p->mode));
            } else if (strcmp(optarg, "paralelo") == 0) {
                strncpy(p->mode, optarg, sizeof(p->mode));
            } else if (strcmp(optarg, "justo") == 0) {
                strncpy(p->mode, optarg, sizeof(p->mode));
            } else {
                errx(EXIT_FAILURE, "mode \"%s\" not valid", optarg);
            }
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

void print_reg(int id, char *type_cl, char *tp) {

    int i;

    struct timespec reg_time;

    if (clock_gettime(CLOCK_REALTIME, &reg_time) < 0) {
        err(EXIT_FAILURE, "clock_gettime failed");
    }

    printf("[%ld.%09ld] Nuevo cliente (%d) %s conectado :\n%s\nResumen:\n",
            reg_time.tv_sec, reg_time.tv_nsec, id, type_cl, tp);

    for (i = 0; i < MAX_TOPICS; i++) {
        if (topics[i][0] != '\0') {
            printf("\t%s: %d Suscriptores - %d Publicadores\n", topics[i], subs_tps[i], pubs_tps[i]);
        }
    }
    printf("\n");
}

void print_unreg(int id, char *topic, char *type_cl) {

    int i;

    struct timespec unreg_time;

    if (clock_gettime(CLOCK_REALTIME, &unreg_time) < 0) {
        err(EXIT_FAILURE, "clock_gettime failed");
    }

    printf("[%ld.%09ld] Eliminado cliente (%d) %s: %s\n Resumen:\n",
            unreg_time.tv_sec, unreg_time.tv_nsec, id, type_cl, topic);
    for (i = 0; i < MAX_TOPICS; i++) {
        if (topics[i][0] != '\0') {
            printf("\t%s: %d Suscriptores - %d Publicadores\n", topics[i], subs_tps[i], pubs_tps[i]);
        }
    }
    printf("\n");
}

int find_topic(char *tp) {
    
    int i;

    for (i = 0; i < MAX_TOPICS; i++) {
        if (strcmp(topics[i], tp) == 0) {
            return i;
        }
    }

    return -1;
}

void print_pub_msg(char * topic) {

    struct timespec pub_time;
    int ind_top;

    ind_top = find_topic(topic);

    if (clock_gettime(CLOCK_REALTIME, &pub_time) < 0) {
        err(EXIT_FAILURE, "clock_gettime failed");
    }

    printf("\n[%ld.%09ld] Enviando mensaje en topic %s a %d suscriptores.\n",
            pub_time.tv_sec, pub_time.tv_nsec, topic, subs_tps[ind_top]);
}

void print_recv_msg(struct message msg) {

    struct timespec recv_time;

    if (clock_gettime(CLOCK_REALTIME, &recv_time) < 0) {
        err(EXIT_FAILURE, "clock_gettime failed");
    }

    printf("\n[%ld.%09ld] Recibido mensaje para publicar en topic: %s - mensaje: %s - Generó: %ld.%09ld\n",
            recv_time.tv_sec, recv_time.tv_nsec, msg.topic, msg.data.data,
            msg.data.time_generated_data.tv_sec, msg.data.time_generated_data.tv_nsec);
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

int find_slot_tp() {

    int i;

    for (i = 0; i < MAX_TOPICS; i++) {
        if (topics[i][0] == '\0') {
            return i;
        }
    }

    return -1;
}

void *send_msg_parallel(void *data2subs) {

    Data2Subs *p_data2subs = data2subs;

    send_msg((*p_data2subs).fd, (*p_data2subs).msg);

    pthread_exit(NULL);
}

void *send_msg_just(void *data2Subs) {

    Data2Subs *p_data2subs = data2Subs;

    pthread_barrier_wait (&barrier);
    send_msg((*p_data2subs).fd, (*p_data2subs).msg);

    pthread_exit(NULL);
}
  
void *managing_pub(void *id) {

    int ind_top, *p_id = id, i;
    struct message msg_pub, msg2subs;
    struct response resp_pub;
    Data2Subs data2subs[MAX_SUBS];
    pthread_t thr_cls[MAX_SUBS];
    
    Node *node_pub = get_IDnode(*p_id, &head_pub, &mtx_pubs), *temp;

    if (node_pub == NULL) {
        pthread_exit(NULL);
    }

    while(1) {
        
        receive_msg(node_pub->data.fd, &msg_pub, 0);

        if (msg_pub.action == UNREGISTER_PUBLISHER) {
            ind_top = find_topic(msg_pub.topic);
            pubs_tps[ind_top]--;
            if (pubs_tps[ind_top] == 0 && subs_tps[ind_top] == 0) {
                memset(topics[ind_top], '\0', MAX_TXT);
            }
            resp_pub.id = *p_id;
            resp_pub.response_status = OK;
            send_resp(node_pub->data.fd, resp_pub);
            print_unreg(msg_pub.id, msg_pub.topic, "Publicador");
            
            deleteNd_N(*p_id, &head_pub, &last_pub, &mtx_pubs);
            
            ids_pubs[*p_id] = -1;
            if (!full_pubs) {
                id_p--;
                if (id_p == 0) {
                    init_pub = 1;
                }
            }
            
            pthread_exit(NULL);
        } else {
            msg2subs.action = PUBLISH_DATA;
            strncpy(msg2subs.data.data, msg_pub.data.data, MAX_TXT);
            strncpy(msg2subs.topic, msg_pub.topic, MAX_TXT);
            msg2subs.data.time_generated_data = msg_pub.data.time_generated_data;
            temp = head_subs;

            print_recv_msg(msg2subs);
            print_pub_msg(msg_pub.topic);

            if (strcmp(params.mode, "secuencial") == 0) {
            
                while(temp != NULL) {

                    if (strcmp(temp->data.topic, msg_pub.topic) == 0) {
                        send_msg(temp->data.fd, msg2subs);
                    }

                    temp = temp->next;
                }
            } else if (strcmp(params.mode, "paralelo") == 0) {
                ind_top = find_topic(msg_pub.topic);
                i = 0;
                
                while(temp != NULL || i < subs_tps[ind_top]) {

                    if (strcmp(temp->data.topic, msg_pub.topic) == 0) {
                        data2subs[i].msg = msg2subs;
                        data2subs[i].fd = temp->data.fd;
                        if (pthread_create(&thr_cls[i], NULL, send_msg_parallel, &data2subs[i]) != 0) {
                            warnx("error creating thread");
                            pthread_exit(NULL);
                        }
                        temp = temp->next;
                        i++;
                        continue;
                    }

                    temp = temp->next;
                }
                for (i = 0; i < subs_tps[ind_top]; i++) {
                    if (pthread_join(thr_cls[i], NULL) != 0) {
                        pthread_exit(NULL);
                    }
                }
            } else if (strcmp(params.mode, "justo") == 0) {
                ind_top = find_topic(msg_pub.topic);

                pthread_barrier_init(&barrier, NULL, subs_tps[ind_top]);

                i = 0;

                while(temp != NULL || i < subs_tps[ind_top]) {

                    if (strcmp(temp->data.topic, msg_pub.topic) == 0) {
                        data2subs[i].msg = msg2subs;
                        data2subs[i].fd = temp->data.fd;
                        data2subs[i].n = i;
                        data2subs[i].n_subs_topic = subs_tps[ind_top];
                        if (pthread_create(&thr_cls[i], NULL, send_msg_just, &data2subs[i]) != 0) {
                            warnx("error creating thread");
                            pthread_exit(NULL);
                        }
                        temp = temp->next;
                        i++;
                        continue;
                    }

                    temp = temp->next;
                }

                for (i = 0; i < subs_tps[ind_top]; i++) {
                    if (pthread_join(thr_cls[i], NULL) != 0) {
                        pthread_exit(NULL);
                    }
                }
                pthread_barrier_destroy(&barrier);
            }
        }
    }
    pthread_exit(NULL);
}

void *managing_subs(void *id) {

    int ind_top, *p_id = id;
    struct message msg_subs_unreg;
    struct response resp_subs;

    Node *node_subs = get_IDnode(*p_id, &head_subs, &mtx_subs);

    if (node_subs == NULL) {
        pthread_exit(NULL);
    }

    while(1) {

        receive_msg(node_subs->data.fd, &msg_subs_unreg, 0);

        if (msg_subs_unreg.action == UNREGISTER_SUBSCRIBER) {
            ind_top = find_topic(msg_subs_unreg.topic);
            subs_tps[ind_top]--;
            if (pubs_tps[ind_top] == 0 && subs_tps[ind_top] == 0) {
                memset(topics[ind_top], '\0', MAX_TXT);
            }
            resp_subs.id = *p_id;
            resp_subs.response_status = OK;
            send_resp(node_subs->data.fd, resp_subs);
            print_unreg(msg_subs_unreg.id, msg_subs_unreg.topic, "Suscriptor");
            
            deleteNd_N(*p_id, &head_subs, &last_subs, &mtx_subs);
            
            ids_subs[*p_id-1] = -1;
            if (!full_subs) {
                id_s--;
                if (id_s == 0) {
                    init_subs = 1;
                }
            }
            
            pthread_exit(NULL);
        }
    }
}

void handler_finish_broker() {

    int i;

    for(i = 0; i < MAX_PUBS; i++) {
        deleteNd_N(i, &head_pub, &last_pub, &mtx_pubs);
    }

    for(i = 0; i < MAX_SUBS; i++) {
        deleteNd_N(i+1, &head_subs, &last_subs, &mtx_subs);
    }
    
    for (i = 0; i < MAX_TOPICS; i++) {
        free(topics[i]);
    }

    pthread_mutex_destroy(&mtx_pubs);
    pthread_mutex_destroy(&mtx_subs);
    
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {

    char *type_cl;
    int cl_fd, i, state_reg, state_tp, full_tps = 0;
    
    Data data;

    struct message msg_register;
    struct response resp_register;

    pthread_t thr_pubs[MAX_PUBS];
    pthread_t thr_subs[MAX_SUBS];

    setbuf(stdout, NULL);
    memset(subs_tps, 0, MAX_TOPICS*sizeof(subs_tps[0]));
    memset(pubs_tps, 0, MAX_TOPICS*sizeof(pubs_tps[0]));

    for (i = 0; i < MAX_TOPICS; i++) {
        topics[i] = malloc(sizeof(char)*MAX_TXT);
    }

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: --port PORT --mode secuencial/paralelo/justo");
    }
    save_params(argc, argv, &params);

    init_serv("0.0.0.0", params.port);

    signal(SIGINT, handler_finish_broker);

    while (1) {
        // Esperar a un publicador o subscriptor
        wait_cl(&cl_fd);

        // Registrar publicador o subscriptor
        state_reg = receive_msg(cl_fd, &msg_register, 0);
        if (state_reg < 0) {
            resp_register.response_status = ERROR;
            resp_register.id = -1;
            send_resp(cl_fd, resp_register);
            continue;
        }

        if (counter_tp >= MAX_TOPICS) {
            full_tps = 1;
            counter_tp = find_slot_tp();
            if (counter_tp == -1) {
                counter_tp = MAX_TOPICS;
                resp_register.response_status = LIMIT;
                resp_register.id = -1;
                send_resp(cl_fd, resp_register);
                continue;
            }
        }
        
        state_tp = find_topic(msg_register.topic);
        if (state_tp == -1) {
            strncpy(topics[counter_tp], msg_register.topic, MAX_TXT);
            if (!full_tps) {
                counter_tp++;
            }
        }
        
        if (msg_register.action == REGISTER_PUBLISHER) {
            if (id_p >= MAX_PUBS) {
                full_pubs = 1;
                id_p = find_slot(ids_pubs, MAX_PUBS);
                if (id_p == -1) {
                    id_p = MAX_PUBS;
                    resp_register.response_status = LIMIT;
                    resp_register.id = -1;
                    send_resp(cl_fd, resp_register);
                    close(cl_fd);
                    continue;
                }
            }
            type_cl = "Publicador";
            data.fd = cl_fd;
            data.id = id_p;
            strncpy(data.topic, msg_register.topic, MAX_TXT);
            
            if (init_pub) {
                init_pub = 0;
                createdList(data, &head_pub, &last_pub, &mtx_pubs);
            } else {
                insertNdAtEnd(data, &last_pub, &mtx_pubs);
            }

            if (state_tp == -1) {
                pubs_tps[counter_tp-1]++;
            } else {
                pubs_tps[state_tp]++;
            }
            
            resp_register.response_status = OK;
            resp_register.id = id_p;
            ids_pubs[id_p] = id_p;
            
            send_resp(cl_fd, resp_register);
            
            if (pthread_create(&thr_pubs[id_p], NULL, managing_pub, &ids_pubs[id_p]) != 0) { //  threads are created
                warnx("error creating thread");
                exit(EXIT_FAILURE);
            }

            if (!full_pubs) {
                id_p++;
            } else {
                id_p = MAX_PUBS;
            }
        } else if (msg_register.action == REGISTER_SUBSCRIBER) {
            if (id_s >= MAX_SUBS) {
                id_s = find_slot(ids_subs, MAX_SUBS);
                full_subs = 1;
                if (id_s == -1) {
                    id_s = MAX_SUBS;
                    resp_register.response_status = LIMIT;
                    resp_register.id = -1;
                    send_resp(cl_fd, resp_register);
                    close(cl_fd);
                    continue;
                }
            }
            type_cl = "Suscriptor";
            data.fd = cl_fd;
            data.id = id_s+1;
            strncpy(data.topic, msg_register.topic, MAX_TXT);
            
            if (init_subs) {
                init_subs = 0;
                createdList(data, &head_subs, &last_subs, &mtx_subs);
            } else {
                insertNdAtEnd(data, &last_subs, &mtx_subs);
            }

            if (state_tp == -1) {
                subs_tps[counter_tp-1]++;
            } else {
                subs_tps[state_tp]++;
            }
            
            resp_register.response_status = OK;
            resp_register.id = id_s+1;
            ids_subs[id_s] = id_s+1;
            
            send_resp(cl_fd, resp_register);
            
            if (pthread_create(&thr_subs[id_s], NULL, managing_subs, &ids_subs[id_s]) != 0) { //  threads are created
                warnx("error creating thread");
                exit(EXIT_FAILURE);
            }

            if (!full_subs) {
                id_s++;
            } else {
                id_s = MAX_SUBS;
            }
        }

        print_reg(resp_register.id,type_cl,msg_register.topic);
        printf("\n");

        if (state_tp != -1 && pubs_tps[state_tp] == 0 && subs_tps[state_tp] == 0) {
            memset(topics[state_tp], 0, MAX_TXT);
        }

    }
    
    exit(EXIT_SUCCESS);
}
