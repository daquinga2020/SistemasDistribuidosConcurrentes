#include "proxy.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>

#define MAX_TXT 100

struct Params
{
    int port;
    char ip[16];
    char topic[MAX_TXT];
};
typedef struct Params Params;

typedef void (*sighandler_t)(int);

Params params;
int cl_fd;
int id;
int action;

void handler_unreg() {

    struct message msg_unreg;
    struct response resp_unreg;
    struct timespec unreg_time;

    msg_unreg.action = UNREGISTER_SUBSCRIBER;
    strncpy(msg_unreg.topic, params.topic, sizeof(msg_unreg.topic));
    msg_unreg.id = id;
    
    send_msg(cl_fd, msg_unreg);
    receive_resp(cl_fd, &resp_unreg);

    if (clock_gettime(CLOCK_MONOTONIC, &unreg_time) < 0) { //  take the final time
        err(EXIT_FAILURE, "clock_gettime failed");
    }

    printf("[%ld.%09ld] De-Registrado (%d) correctamente del broker.\n", unreg_time.tv_sec, unreg_time.tv_nsec, resp_unreg.id);
    close(cl_fd);

    exit(EXIT_SUCCESS);
}

int check_nargs(int n) {

    return n == 6;
}

void save_params(int n, char **args, Params *p) {
    
    int c;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"port", required_argument, 0, 'p'},
            {"ip", required_argument, 0, 'i'},
            {"topic", required_argument, 0, 't'},
        };

        c = getopt_long (n, args, "p:m:t:",
                long_options, &option_index);

        if (c == -1)
           break;

        switch (c) {
        case 'p':
            p->port = atoi(optarg);
            break;

        case 'i':
            strncpy(p->ip, optarg, sizeof(p->ip));
            break;
        
        case 't':
            strncpy(p->topic, optarg, sizeof(p->topic));
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

int main(int argc, char *argv[]) {

    struct message msg_reg, msg_from_pub;
    struct response resp;
    struct timespec timestamp, time_received_data;
    double diff_time_sec, diff_time_nsec, latency;
    char data[MAX_TXT], inf[MAX_TXT*10];

    setbuf(stdout, NULL);

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: --ip IP --port PORT --topic TOPIC");
    }
    save_params(argc, argv, &params);

    msg_reg.action = REGISTER_SUBSCRIBER;
    strncpy(msg_reg.topic, params.topic, sizeof(msg_reg.topic));
    
    connect2serv(params.ip, params.port, &cl_fd);

    send_msg(cl_fd, msg_reg);

    if (clock_gettime(CLOCK_REALTIME, &timestamp) < 0) {
        err(EXIT_FAILURE, "clock_gettime failed");
    }
    printf("[%ld.%09ld] Subscriber conectado con el broker correctamente.\n", timestamp.tv_sec, timestamp.tv_nsec);

    receive_resp(cl_fd, &resp);

    signal(SIGINT, handler_unreg);

    if (clock_gettime(CLOCK_REALTIME, &timestamp) < 0) {
        err(EXIT_FAILURE, "clock_gettime failed");
    }
    if (resp.id != -1) {
        id = resp.id;
        printf("[%ld.%09ld] Registrado correctamente con ID: %d para topic %s\n", timestamp.tv_sec, timestamp.tv_nsec, resp.id, params.topic);
    } else {
        fprintf(stderr, "[%ld.%09ld] Error al hacer el registro: %d\n", timestamp.tv_sec, timestamp.tv_nsec, resp.response_status);
        close(cl_fd);
        exit(EXIT_FAILURE);
    }
    
    while(1) {
        receive_msg(cl_fd, &msg_from_pub, 0);
        if (clock_gettime(CLOCK_REALTIME, &time_received_data) < 0) {
            err(EXIT_FAILURE, "clock_gettime failed");
        }
        strncpy(data, msg_from_pub.data.data, MAX_TXT);
        
        diff_time_sec = time_received_data.tv_sec - msg_from_pub.data.time_generated_data.tv_sec;
        diff_time_nsec = (time_received_data.tv_nsec - msg_from_pub.data.time_generated_data.tv_nsec)*10e-9;
        
        latency = diff_time_sec + diff_time_nsec;
        
        if (clock_gettime(CLOCK_REALTIME, &timestamp) < 0) {
            err(EXIT_FAILURE, "clock_gettime failed");
        }
        snprintf(inf, MAX_TXT*10, "[%ld.%09ld] Recibido mensaje topic: %s - mensaje: %s - Generó: %ld.%09ld - Recibido: %ld.%09ld - Latencia: %f.\n",
                timestamp.tv_sec, timestamp.tv_nsec, msg_from_pub.topic, data,
                msg_from_pub.data.time_generated_data.tv_sec, msg_from_pub.data.time_generated_data.tv_nsec,
                time_received_data.tv_sec, time_received_data.tv_nsec, latency);

        printf("%s", inf);
    }

    exit(EXIT_SUCCESS);
}