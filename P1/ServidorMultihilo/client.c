#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <err.h>

#ifdef DEBUG
    #define DEBUG_PRINTF(...) printf("DEBUG: "__VA_ARGS__)
#else
    #define DEBUG_PRINTF(...)
#endif

#define MAX_LINE 256

typedef void (*sighandler_t)(int);

int client_fd;

void
handler_cl (int number) {

    close(client_fd);
    exit(EXIT_SUCCESS);
}

int
check_nargs(int n) {
    
    return n == 3;
}

int main(int argc, char *argv[]) {

    int err_connect, port;
    char msg_sv[MAX_LINE], msg_client[MAX_LINE], *id, *ip;
    struct sockaddr_in claddr;

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: client_id ip port");
    }
    
    id = argv[1];
    ip = argv[2];
    port = atoi(argv[3]);
    
    snprintf(msg_client, MAX_LINE, "Hello server! From client: %s\n", id);

    setbuf(stdout, NULL);
    
    // Creating socket file descriptor
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        err(EXIT_FAILURE, "socket failed");
    }
    
    printf("Socket successfully created...\n");
    
    // Assign IP, SERV_PORT, IPV4 to client
    claddr.sin_family = AF_INET;
    //  Put IP of localhost
    claddr.sin_addr.s_addr = inet_addr(ip);
    claddr.sin_port = htons(port);

    // Connecting to the server
    err_connect = connect(client_fd, (struct sockaddr *)&claddr, sizeof(claddr));
    if (err_connect < 0) {
        err(EXIT_FAILURE, "connect failed");
    }
    
    printf("conected to the server...\n");

    signal(SIGINT, handler_cl);

    // Sending msg to server
    if (send(client_fd, msg_client, sizeof(msg_client), 0) < 0) {
        err(EXIT_FAILURE,"send failed");
    }
    
    //  Waiting msg from server
    if (recv(client_fd, msg_sv, sizeof(msg_sv), 0) < 0) {
        err(EXIT_FAILURE, "recv failed");
    }
    printf("+++ %s\n", msg_sv);

    // Close client
    close(client_fd);

    exit(EXIT_SUCCESS);
}