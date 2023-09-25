#include "proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

int
check_nargs(int n) {

    return n == 2;
}

int main(int argc, char *argv[]) {

    int port;
    char *ip;
    struct message msg;

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: ip port");
    }

    setbuf(stdout, NULL);

    ip = argv[1];
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

    close_cl();

    exit(EXIT_SUCCESS);
}