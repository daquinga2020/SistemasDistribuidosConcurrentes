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
struct request rq;

int check_nargs(int n) {

    return n == 8;
}

void save_params(int n, char **args, Params *p) {
    //int digit_optind = 0;
    int c;

    while (1) {
        //int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"ip", required_argument, 0, 'i'},
            {"port", required_argument, 0, 'p'},
            {"mode", required_argument, 0, 'm'},
            {"threads", required_argument, 0, 't'},
        };

        c = getopt_long (n, args, "p:r:",
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
            strncpy(p->mode, optarg, sizeof(p->mode));
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

// Cada hilo entra aqui para enviar el mensaje
void *sending_msg(void *fd) {

    int *p_fd = fd;
    printf("HILO ENVIA MENSAJE\n");
    send_rq(*p_fd, &rq);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

    int i, *cl_fds;
    pthread_t *thrds;
    
    setbuf(stdout, NULL);

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: --ip IP --port PORT --mode writer/reader --threads n");
    }
    save_params(argc, argv, &params);
    
    if (strcmp(params.mode, "writer") == 0) {
        rq.action = WRITE;
    } else if (strcmp(params.mode, "reader") == 0) {
        rq.action = READ;
    } else {
        errx(EXIT_FAILURE, "mode \"%s\" not valid", params.mode);
    }

    thrds = malloc(sizeof(pthread_t) * params.threads);    // Cambiar por un puntero con malloc a lo mejor
    cl_fds = malloc(sizeof(int)*params.threads); // Cambiar por un puntero con malloc a lo mejor

    
    // Crear un socket por cada hilo para generar una conexion/hilo
    for (i = 0; i < params.threads; i++) {
        
        connect2serv(params.ip, params.port, &cl_fds[i]);
        if (pthread_create(&thrds[i], NULL, sending_msg, &cl_fds[i]) != 0) {
            warnx("error creating thread");
            exit(EXIT_FAILURE);
        }
        
        printf("VUELTA\n");
    }

    // Envio la peticion al servidor creando hilos escritores
    /*for (i = 0; i < params.threads; i++) {

        printf("DESCRIPTOR DE FICHERO PARA HILOS WRITER/READER:%d\n", cl_fds[i]);
        if (pthread_create(&thrds[i], NULL, sending_msg, &cl_fds[i]) != 0) {
            warnx("error creating thread");
            exit(EXIT_FAILURE);
        }
    }*/

    

    free(thrds);
    free(cl_fds);

    /*ip = argv[1];
    port = atoi(argv[2]);

    set_name("P1");

    // Establish communication with P2
    set_ip_port(ip, port);
    connect2serv();

    // First message to P2: Ready to shutdown (READY_TO_SHUTDOWN)
    notify_ready_shutdown();

    // Receive message from P2 and send shutdown ACK to P2(SHUTDOWN_ACK)
    wait_msg2serv();
    notify_shutdown_ack();

    close_cl();*/

    exit(EXIT_SUCCESS);
}