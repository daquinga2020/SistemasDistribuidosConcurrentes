#include "proxy.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <err.h>

#define MAX_TXT 64

struct Params
{
    int port;
    int threads;
    char ip[MAX_TXT];
    char mode[MAX_TXT];
};
typedef struct Params Params;

Params params;
int *ids;
int action;

int check_nargs(int n) {

    return n == 8;
}

void save_params(int n, char **args, Params *p) {

    int c;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"ip", required_argument, 0, 'i'},
            {"port", required_argument, 0, 'p'},
            {"mode", required_argument, 0, 'm'},
            {"threads", required_argument, 0, 't'},
        };

        c = getopt_long (n, args, "p:i:m:t:",
                long_options, &option_index);

        if (c == -1)
           break;

        switch (c) {
        case 'i':
            strncpy(p->ip, optarg, sizeof(p->ip));
            break;
        
        case 'p':
            p->port = atoi(optarg);
            break;

        case 'm':
            if (strcmp(optarg, "writer") == 0) {
                strncpy(p->mode, "ESCRITOR", sizeof(p->mode));
                action = WRITE;
            } else if (strcmp(optarg, "reader") == 0) {
                strncpy(p->mode, "LECTOR", sizeof(p->mode));
                action = READ;
            } else {
                errx(EXIT_FAILURE, "mode \"%s\" not valid", optarg);
            }
            break;

        case 't':
            p->threads = atoi(optarg);
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

void *sending_rq(void *ind) {

    int *p_id = ind, fd;
    struct request rq;
    struct response resp;

    rq.id = *p_id;
    rq.action = action;

    connect2serv(params.ip, params.port, &fd);

    send_request(fd, rq);

    receiving_rps(fd, &resp);

    printf("[CLIENTE #%d] %s, contador=%d, tiempo=%09ld ns.\n",
            rq.id, params.mode, resp.counter, resp.waiting_time);

    close(fd);
    
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

    int i;
    pthread_t *thrds;
    
    setbuf(stdout, NULL);

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: --ip IP --port PORT --mode writer/reader --threads n");
    }
    save_params(argc, argv, &params);

    thrds = malloc(sizeof(pthread_t) * params.threads);
    ids = malloc(sizeof(int) * params.threads);
    
    for (i = 0; i < params.threads; i++) {
        ids[i] = i;
        if (pthread_create(&thrds[i], NULL, sending_rq, &ids[i]) != 0) {
            warnx("error creating thread");
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < params.threads; i++) {
        if (pthread_join(thrds[i], NULL) != 0) {
            warnx("error joining thread");
            exit(EXIT_FAILURE);
        }
    }

    free(thrds);
    free(ids);

    exit(EXIT_SUCCESS);
}