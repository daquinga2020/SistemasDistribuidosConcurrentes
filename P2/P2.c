#include "proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

int
check_nargs(int n) {

    return n == 2;
}

int main(int argc, char *argv[]) {

    int port, nclients = 2, msg_rcv, i, lamport;
    char *ip;
    struct message msg;
    
    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: ip port");
    }

    ip = argv[1];
    port = atoi(argv[2]);

    set_name("P2");
    set_ip_port(ip, port);
    
    if (init_serv() < 0) {
        exit(EXIT_FAILURE);
    }

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

    printf("Los clientes fueron correctamente apagados en t(lamport) = %d\n", get_clock_lamport());

    exit(EXIT_SUCCESS);
}