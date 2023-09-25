#include "proxy.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define _GNU_SOURCE
//#include <sys/types.h>
#include <sys/socket.h>
#include <getopt.h>
#include <err.h>

#define MAX_TXT 100
#define MAX_TOPICS 10
#define MAX_PUBS 5 //100 
#define MAX_SUSCR 15 //900

struct NodePub
{
    DataPub dpb;
    NodePub *prev;
    NodePub *next;
};

struct NodeSuscr
{
    DataSuscr dsr;
    NodeSuscr *prev;
    NodeSuscr *next;
};

struct Params
{
    int port;
    char mode[MAX_TXT];
};
// https://gist.github.com/ArgiesDario/da409828e81ef441186268b8ee3acd5f
struct DataPub
{
    int id;
    int fd;
    char topic[MAX_TXT];
    
    /*int ids_pubs[MAX_PUBS];
    int pubs[MAX_PUBS];
    int pubs_tps[MAX_TOPICS];
    int counter_p;
    int full_pubs;*/
};

struct DataSuscr
{
    int ids_suscr[MAX_SUBS];
    int suscr[MAX_SUBS];
    int suscr_tps[MAX_TOPICS];
    int counter_s;
    int full_suscr;
};

typedef struct Params Params;
typedef struct DataPub DataPub;
typedef struct DataSuscr DataSuscr;
typedef struct NodePub NodePub;
typedef struct NodeSuscr NodeSuscr;

Params params;
NodePub *head;
NodePub *last;
NodeSuscr *head_suscr;
NodeSuscr *last_suscr;
int ids_pubs[MAX_PUBS];
int ids_suscr[MAX_SUBS];
int pubs[MAX_PUBS];
int suscr[MAX_SUBS];
int pubs_tps[MAX_TOPICS];
int subs_tps[MAX_TOPICS];
int action;
int counter_tp = 0;
int counter_p = 0;
int counter_s = 0;
int full_pubs = 0;
int full_subs = 0;

char *topics[MAX_TOPICS];

void createdPubList(DataPub dpb) {

    NodePub *new_npb;

    head = (NodePub *)malloc(sizeof(NodePub));

    if (head == NULL) {
        err(EXIT_FAILURE,"malloc failed");
    }
    /*dpb.id = 0;
    memset(dpb.topic, '\0', MAX_TXT);*/

    head->dpb = dpb;
    head->prev = NULL;
    head->next = NULL;

    last = head;

    new_npb = (NodePub *)malloc(sizeof(NodePub));

    if (new_npb == NULL) {
        err(EXIT_FAILURE, "malloc failed");
    }
}

void createdSuscrList(DataSuscr dsr) {

    NodeSuscr *new_nsuscr;

    head_suscr = (NodeSuscr *)malloc(sizeof(NodeSuscr));

    if (head_suscr == NULL) {
        err(EXIT_FAILURE,"malloc failed");
    }
    
    head_suscr->dsr = dsr;
    head_suscr->prev = NULL;
    head_suscr->next = NULL;

    last_suscr = head_suscr;

    new_nsuscr = (NodeSuscr *)malloc(sizeof(NodeSuscr));

    if (new_nsuscr == NULL) {
        err(EXIT_FAILURE, "malloc failed");
    }
}

void insertNdPubAtEnd(DataPub dpb) {
    
    NodePub *new_npb;

    if(last == NULL) {
        printf("Error, List is empty!\n"); // Cambiarlo 
    } else {
        new_npb = (NodePub *)malloc(sizeof(NodePub));

        new_npb->dpb = dpb;
        new_npb->next = NULL;
        new_npb->prev = last;

        last->next = new_npb;
        last = new_npb;

        printf("NODE INSERTED SUCCESSFULLY AT THE END OF LIST\n"); // Quitarlo
    }
}

void PrintListPubFromFirst() {
    NodePub * temp;
    int n = 1;

    if (head == NULL) {
        printf("NodePub list is empty."); // Cambiarlo 
    } else {
        temp = head;
        printf("\n\nDATA IN THE LIST:\n");

        while(temp != NULL)
        {
            printf("DATA of %d node: ID: %d Topic: %s FD: %d\n", n, temp->dpb.id, temp->dpb.topic, temp->dpb.fd);

            n++;
            
            /* Move the current pointer to next node */
            temp = temp->next;
        }
    }
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

    printf("[%d.%d] Nuevo cliente (%d) %s conectado :\n%s\nResumen:\n",
            0, 0, id, type_cl, tp);

    for (i = 0; i < MAX_TOPICS; i++) {
        if (topics[i][0] != '\0') {
            printf("\t%s: %d Suscriptores - %d Publicadores\n", topics[i], subs_tps[i], pubs_tps[i]);
        }
    }
    printf("\n");
}

void print_unreg(int id, char *topic, char *type_cl) {

    int i;

    printf("[%d.%d] Eliminado cliente (%d) %s: %s\n Resumen:\n", 0, 0, id, type_cl, topic);
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

int find_slot_fd(int *fds, int max_fds) {

    int i;

    for (i = 0; i < max_fds; i++) {
        if (fds[i] == -1) {
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

void *managing_pub(void *id) {

    int ind_top, *p_id = id;
    struct message msg_pub;
    struct response resp_pub;

    while(1) {
        receive_msg(pubs[*p_id], &msg_pub, 0);

        if (msg_pub.action == UNREGISTER_PUBLISHER) {
            ind_top = find_topic(msg_pub.topic);
            pubs_tps[ind_top]--;
            if (pubs_tps[ind_top] == 0 && subs_tps[ind_top] == 0) {
                memset(topics[ind_top], 0, MAX_TXT);
            }
            resp_pub.id = *p_id;
            resp_pub.response_status = OK;  
            send_resp(pubs[*p_id], resp_pub);
            print_unreg(msg_pub.id, msg_pub.topic, "Publicador");
            pubs[*p_id] = -1;
            if (!full_pubs) {
                counter_p--;
            }
            pthread_exit(NULL);
        }
    }
    pthread_exit(NULL);
}

void *managing_subs(void *id) {

    int ind_top, *p_id = id;
    struct message msg_suscr_unreg;
    struct message msg2suscr;
    struct response resp_suscr;

    while(1) {
        receive_msg(suscr[*p_id], &msg_suscr_unreg, MSG_DONTWAIT);

        if (msg_suscr_unreg.action == UNREGISTER_SUBSCRIBER) {
            ind_top = find_topic(msg_suscr_unreg.topic);
            subs_tps[ind_top]--;
            if (pubs_tps[ind_top] == 0 && subs_tps[ind_top] == 0) {
                memset(topics[ind_top], 0, MAX_TXT);
            }
            resp_suscr.id = *p_id;
            resp_suscr.response_status = OK;  
            send_resp(suscr[*p_id], resp_suscr);
            print_unreg(msg_suscr_unreg.id, msg_suscr_unreg.topic, "Suscriptor");
            suscr[*p_id] = -1; // Funcion que busque fd -1 y devuelva el indice
            pthread_exit(NULL);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

    char *type_cl;
    int cl_fd;
    int full_tps = 0;
    int state_reg;
    int state_tp;
    int id_p = 0;
    int id_s = 1;

    struct message msg_register;
    struct response resp_register;

    pthread_t thr_pubs[MAX_PUBS];
    pthread_t thr_suscr[MAX_SUBS]; 

    setbuf(stdout, NULL);
    memset(subs_tps, 0, MAX_TOPICS*sizeof(subs_tps[0]));
    memset(pubs_tps, 0, MAX_TOPICS*sizeof(pubs_tps[0]));

    for (int i = 0; i < MAX_TOPICS; i++) {
        topics[i] = malloc(sizeof(char)*MAX_TXT);
    }

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: --port PORT --mode secuencial/paralelo/justo");
    }
    save_params(argc, argv, &params);

    init_serv("0.0.0.0", params.port);

    // La forma en la que se debe hacer el envio de mensajes
    /*if (strcmp(params.mode, "secuencial") == 0) {
        printf("EJECUTO MODO SECUENCIAL\n\n");

    } else if (strcmp(params.mode, "paralelo") == 0) {
        printf("EJECUTO MODO PARALELO\n\n");

    } else {
        printf("EJECUTO MODO JUSTO\n\n");
    }*/

    while (1) {
        // Esperar a un publicador o subscriptor
        wait_cl(&cl_fd);

        // Registrar publicador o subscriptor
        state_reg = receive_msg(cl_fd, &msg_register, 0);
        if (state_reg < 0) {
            // Crear funcion de error
            resp_register.response_status = ERROR;
            resp_register.id = -1;
            send_resp(cl_fd, resp_register);
            //fprintf(stderr, "NO SE PUDO REGISTRAR AL CLIENTE\n");
            continue;
        }

        if (counter_tp >= MAX_TOPICS) {
            full_tps = 1;
            counter_tp = find_slot_tp();
            if (counter_tp == -1) {
                counter_tp = MAX_TOPICS;
                // Crear funcion de error, definir distintos mensajes de error
                resp_register.response_status = LIMIT;
                resp_register.id = -1;
                send_resp(cl_fd, resp_register);
                //fprintf(stderr, "NO SE PUDO REGISTRAR AL CLIENTE\n");
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

            if (counter_p >= MAX_PUBS) {
                counter_p = find_slot_fd(pubs, MAX_PUBS);
                if (counter_p == -1) {
                    // Crear funcion de error
                    counter_p = MAX_PUBS;
                    resp_register.response_status = LIMIT;
                    resp_register.id = -1;
                    send_resp(cl_fd, resp_register);
                    close(cl_fd);
                    continue;
                }
            }
            // Crear funcion
            type_cl = "Publicador";
            pubs[counter_p] = cl_fd;
            resp_register.response_status = OK;
            /*resp_register.id = counter_p;
            ids_pubs[counter_p] = counter_p;*/
            // Todo depende del id
            resp_register.id = id_p;
            ids_pubs[counter_p] = id_p;
            if (state_tp == -1) {
                pubs_tps[counter_tp-1]++;
            } else {
                pubs_tps[state_tp]++;
            }
            send_resp(pubs[counter_p], resp_register);
            if (pthread_create(&thr_pubs[counter_p], NULL, managing_pub, &ids_pubs[counter_p]) != 0) { //  threads are created
                warnx("error creating thread");
                exit(EXIT_FAILURE);
            }
            if (!full_pubs) {
                counter_p++;
            } else {
                counter_p = MAX_PUBS;
            }
             // Comprobar si alcanza el maximo
            if (counter_p == MAX_PUBS) {
                full_pubs = 1;
            }
            id_p++;
        } else if (msg_register.action == REGISTER_SUBSCRIBER) {
            if (counter_s >= MAX_SUBS) {
                counter_s = find_slot_fd(suscr, MAX_SUBS);
                if (counter_s == -1) {
                    counter_s = MAX_SUBS;
                    // Crear funcion de error
                    resp_register.response_status = LIMIT;
                    resp_register.id = -1;
                    send_resp(cl_fd, resp_register);
                    close(cl_fd);
                    continue;
                }
            }
            // Crear funcion
            type_cl = "Suscriptor";
            suscr[counter_s] = cl_fd;
            resp_register.response_status = OK;
            resp_register.id = counter_s+1;
            ids_suscr[counter_s] = counter_s;
            if (state_tp == -1) {
                subs_tps[counter_tp-1]++;
            } else {
                subs_tps[state_tp]++;
            }
            send_resp(suscr[counter_s], resp_register);
            if (pthread_create(&thr_suscr[counter_s], NULL, managing_subs, &ids_suscr[counter_s]) != 0) { //  threads are created
                warnx("error creating thread");
                exit(EXIT_FAILURE);
            }
            if (!full_subs) {
                counter_s++;
            } else {
                counter_s = MAX_SUBS;
            }
            // Comprobar si alcanza el maximo
            if (counter_s == MAX_SUBS) {
                full_subs = 1;
            }
        }

        print_reg(resp_register.id,type_cl,msg_register.topic);
        printf("\n");

        if (state_tp != -1 && pubs_tps[state_tp] == 0 && subs_tps[state_tp] == 0) {
            memset(topics[state_tp], 0, MAX_TXT);
            /*if (counter_tp > 0) {
                counter_tp--;
            }*/
        }


        /*if (pthread_create(&threads[count], NULL, do_work, &cl_fds[counter]) != 0) { //  threads are created
            warnx("error creating thread");
            exit(EXIT_FAILURE);
        }*/

        /*counter++;
        if (counter >= MAX_PUBS+MAX_SUBS) {
            counter = 0;
        }*/
    }

    for (int i = 0; i < MAX_TOPICS; i++) {
        free(topics[i]);
    }
    
    exit(EXIT_SUCCESS);
}