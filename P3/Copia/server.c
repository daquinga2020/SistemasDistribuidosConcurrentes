#include "proxy.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <err.h>

#define MAX_CLTS 5
#define MAX_TXT 64

int fds[MAX_CLTS];
int writers[MAX_CLTS];
int readers[MAX_CLTS];
//int counter = 0;

struct Params
{
    int port;
    char priority[MAX_TXT];
};
typedef struct Params Params;


int check_nargs(int n) {

    return (n == 4 || n == 6);
}

void save_params(int n, char **args, Params *p) {

    //int digit_optind = 0;
    int c;

    while (1) {
        //int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"port", required_argument, 0, 'p'},
            {"priority", required_argument, 0, 'u'},
            {"ratio", optional_argument, 0, 'r'}
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
            strncpy(p->priority, optarg, sizeof(p->priority));
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

/*void  *waiting_clients() {

    while(1) {
        if (counter < MAX_CLTS) {
            wait_cl(&fds[counter]);
            printf("CONSEGUI CLIENTE #%d#\n", counter);
            counter++;
        }
    }

    pthread_exit(NULL);
}*/

void *waiting_msgs(void *fd) {

    struct request rq;
    int *p_fd = fd;
    
    receiving_msgs(*p_fd, &rq);
    if (rq.action == WRITE) {
        printf("MENSAJE DE ESCRITOR\n");
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

    int counter = 0;
    Params params;
    //pthread_t thr_wait;
    pthread_t thr_recv[MAX_CLTS];

    setbuf(stdout, NULL);

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: --port PORT --priority writer/reader");
    }

    save_params(argc, argv, &params);

    if (init_serv("0.0.0.0", params.port) < 0) {
        exit(EXIT_FAILURE);
    }

    // Hilo para la espera de clientes
    /*if (pthread_create(&thr_wait, NULL, waiting_clients, NULL) != 0) {
        warnx("error creating thread");
        exit(EXIT_FAILURE);
    }

    // Hilo para la recepcion de peticion de clientes escritores o lectores
    if (pthread_create(&thr_recv, NULL, waiting_msgs, NULL) != 0) {
        warnx("error creating thread");
        exit(EXIT_FAILURE);
    }*/

    while (1)
    {
        wait_cl(&fds[counter]); // Espero clientes

        // Recibo mensajes de clientes y hay que filtrarlos segun escritor y lector
        if (pthread_create(&thr_recv[counter], NULL, waiting_msgs, &fds[counter]) != 0) {
            warnx("error creating thread");
            exit(EXIT_FAILURE);
        }
        counter++;
        if (counter >= 5) {
            counter = 0;
        }
    }
    

    //close_serv();

    /*int port, nclients = 2, msg_rcv, i, lamport;
    char *ip;
    struct message msg;

    set_name("P2");
    set_ip_port(ip, port);

    //Receive messages from P1 and P3
    wait_cl();
    wait_cl();

    // Send the shutdown now message to P1
    notify_shutdown_now();

    // Wait for message from P1
    wait_msg2client();

    // Send the shutdown now message to P3
    notify_shutdown_now();

    // Wait for message from P3
    wait_msg2client();

    close_serv();

    printf("Los clientes fueron correctamente apagados en t(lamport) = %d\n", get_clock_lamport());*/

    exit(EXIT_SUCCESS);
}