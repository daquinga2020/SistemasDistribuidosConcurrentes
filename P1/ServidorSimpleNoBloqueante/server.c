#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
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
#define PORT 8080

typedef void (*sighandler_t)(int);

int server_fd, cl_fd;

void
handler_sv (int number) {
    
    // Close socket
    close(cl_fd);
    close(server_fd);   // Closing server file descriptor

    exit(EXIT_SUCCESS);
}

int main() {

    char msg_sv[MAX_LINE], msg_cl[MAX_LINE];
    struct sockaddr_in servaddr;
    struct sockaddr sclient;
    socklen_t addrlen;
    fd_set readmask;
    struct timeval timeout;

    setbuf(stdout, NULL);

    // Creating socket file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        err(EXIT_FAILURE, "socket failed");
    }

    printf("Socket successfully created...\n");

    // Assign IP, SERV_PORT, IPV4 to server
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // Assigning a name to a socket(IP => port)
    if (bind(server_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        err(EXIT_FAILURE, "bind failed");
    }

    printf("Socket successfully bined...\n");

    // Specifying a willingness to accept incoming connections 
    // and a queue limit for incoming connections
    if (listen(server_fd, 1) < 0) {
        err(EXIT_FAILURE, "listen failed");
    }
    printf("Server listening...\n");

    addrlen = sizeof(sclient);
    cl_fd = accept(server_fd, (struct sockaddr *) &sclient, &addrlen);
    if (cl_fd < 0) {
        err(EXIT_FAILURE, "accept failed");
    }

    signal(SIGINT, handler_sv);

    FD_ZERO(&readmask);
    FD_SET(server_fd, &readmask);

    // Write/read to/from client
    while(1) {
        
        FD_SET(cl_fd, &readmask); // Asigning new descriptor
        FD_SET(STDIN_FILENO, &readmask); // Entrada        
        
        timeout.tv_sec=0; 
        timeout.tv_usec=500000; // Timeout of 0.5 sec.
        if (select(cl_fd+1, &readmask, NULL, NULL, &timeout) < 0) {
            perror("select failed");
            break;
        }

        if (FD_ISSET(cl_fd, &readmask)) {
            //  There are data to read in descriptor
            //  Waiting msg from client
            if (recv(cl_fd, msg_cl, sizeof(msg_cl), 0) < 0) {
                perror("recv failed");
                break;
            }
            printf("+++ %s", msg_cl);
        }
        
        //  Sending msg to client
        printf("\n> ");
        fgets(msg_sv, MAX_LINE, stdin);
        if (send(cl_fd, msg_sv, sizeof(msg_sv), 0) < 0) {
            perror("send failed");
            break;
        }
    }

    // Close socket
    close(cl_fd);
    close(server_fd);   // Closing server file descriptor

    exit(EXIT_FAILURE);
}