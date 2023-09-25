#include "proxy.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>

#define MAX_TXT 100

struct Params
{
    int port;
    char ip[MAX_TXT];
    char topic[MAX_TXT];
};
typedef struct Params Params;

Params params;
int cl_fd;
int id;
int action;
int fd_data;

void handler_unreg() {

    struct message msg_unreg;
    struct response resp_unreg;
    struct timespec timestamp;

    msg_unreg.action = UNREGISTER_PUBLISHER;
    msg_unreg.id = id;
    strncpy(msg_unreg.topic, params.topic, sizeof(msg_unreg.topic));

    send_msg(cl_fd, msg_unreg);
    receive_resp(cl_fd, &resp_unreg);

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) < 0) {
        err(EXIT_FAILURE, "clock_gettime failed");
    }

    printf("[%ld.%09ld] De-Registrado (%d) correctamente del broker.\n",
            timestamp.tv_sec, timestamp.tv_nsec, resp_unreg.id);
    close(cl_fd);
    close(fd_data);
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

    struct message msg_reg, msg2subs;
    struct response resp;
    struct timespec t;
    struct timespec ts;
    int num_bytes;
    char str_data[512];

    ts.tv_nsec = 0;
    ts.tv_sec = 3;

    setbuf(stdout, NULL);

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: --ip IP --port PORT --topic TOPIC");
    }
    save_params(argc, argv, &params);
    
    msg_reg.action = REGISTER_PUBLISHER;
    strncpy(msg_reg.topic, params.topic, sizeof(msg_reg.topic));
    
    connect2serv(params.ip, params.port, &cl_fd);

    send_msg(cl_fd, msg_reg);

    if (clock_gettime(CLOCK_MONOTONIC, &t) < 0) {
        err(EXIT_FAILURE, "clock_gettime failed");
    }
    printf("[%ld.%09ld] Publisher conectado con el broker correctamente.\n", t.tv_sec, t.tv_nsec);

    receive_resp(cl_fd, &resp);

    signal(SIGINT, handler_unreg);

    if (clock_gettime(CLOCK_MONOTONIC, &t) < 0) {
        err(EXIT_FAILURE, "clock_gettime failed");
    }
    if (resp.id != -1) {
        id = resp.id;
        printf("[%ld.%09ld] Registrado correctamente con ID: %d para topic %s\n", t.tv_sec, t.tv_nsec, resp.id, params.topic);
    } else {
        fprintf(stderr,"[%ld.%09ld] Error al hacer el registro: error=%d\n", t.tv_sec, t.tv_nsec, resp.response_status);
        close(cl_fd);
        exit(EXIT_FAILURE);
    }
    
    fd_data = open("/proc/loadavg", O_RDONLY);

    while(1) {

        nanosleep(&ts, NULL);

        msg2subs.action = PUBLISH_DATA;
        while ((num_bytes = read(fd_data, str_data, MAX_TXT)) > 0){}
        if (num_bytes < 0) {
            err(EXIT_FAILURE, "read failure");
        }
        
        strncpy(msg2subs.topic, params.topic, MAX_TXT);
        strncpy(msg2subs.data.data, str_data, MAX_TXT);
        
        msg2subs.data.data[strlen(msg2subs.data.data)-1] = '\0';
        if (clock_gettime(CLOCK_REALTIME, &msg2subs.data.time_generated_data) < 0) {
            err(EXIT_FAILURE, "clock_gettime failed");
        }

        send_msg(cl_fd, msg2subs);

        if (clock_gettime(CLOCK_REALTIME, &t) < 0) {
            err(EXIT_FAILURE, "clock_gettime failed");
        }
        printf("[%ld.%09ld] Publicado mensaje topic: %s - mensaje: %s - Generó: %ld.%09ld\n",
                t.tv_sec, t.tv_nsec, params.topic, msg2subs.data.data,
                msg2subs.data.time_generated_data.tv_sec, msg2subs.data.time_generated_data.tv_nsec);
    }

    exit(EXIT_SUCCESS);
}