#include <pthread.h>
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
#define MAX_CLIENTS 1000
#define NCLIENTS 100

typedef void (*sighandler_t)(int);

int server_fd;

void
handler_sv (int number) {

    close(server_fd);   // Closing server file descriptor

    exit(EXIT_SUCCESS);
}

int
check_nargs(int n) {

    return n == 1;
}

int 
create_sock(int port) {

    int server_fd;
    const int enable = 1;
    struct sockaddr_in servaddr;

    // Creating socket file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return -1;
    }
    printf("Socket successfully created...\n");

    // Assign IP, SERV_PORT, IPV4 to server
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    //  To reuse the same port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt failed");
        return -1;
    }

    // Assigning a name to a socket(IP => port)
    if (bind(server_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        return -1;
    }
    printf("Socket successfully bined...\n");

    // Specifying a willingness to accept incoming connections 
    // and a queue limit for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        return -1;
    }
    printf("Server listening...\n");

    return server_fd;
}


void *
do_work (void *fd) {
    
    char msg_client[MAX_LINE];
    char msg_sv[] = "Hello client!";
    int *client_fd = fd;

    //  Waiting msg from client
    if (recv(*client_fd, msg_client, sizeof(msg_client), 0) < 0) {
        perror("recv failed");
        return NULL;
    }
    printf("> %s", msg_client);

    //  Sending msg to client
    if (send(*client_fd, msg_sv, sizeof(msg_sv), 0) < 0) {
        perror("send failed");
        return NULL;
    }

    return NULL;
}

int main(int argc, char *argv[]) {

    int port, count = 0, cl_fd[NCLIENTS], i;
    struct sockaddr sclient;
    pthread_t threads[NCLIENTS];
    socklen_t addrlen;

    if (!check_nargs(argc-1)) {
        errx(EXIT_FAILURE, "usage: server port");
    }

    port = atoi(argv[1]);

    setbuf(stdout, NULL);

    server_fd = create_sock(port);
    if (server_fd < 0) {
        exit(EXIT_FAILURE);
    }
    
    signal(SIGINT, handler_sv);

    while(1){

        addrlen = sizeof(sclient);
        cl_fd[count] = accept(server_fd, (struct sockaddr *) &sclient, &addrlen);
        if (cl_fd[count] < 0) {
            err(EXIT_FAILURE, "accept failed");
        }

        if (pthread_create(&threads[count], NULL, do_work, &cl_fd[count]) != 0) { //  threads are created
            warnx("error creating thread");
            exit(EXIT_FAILURE);
        }
        
        if (count != 99) {
            count++;
            continue;    
        }

        for (i = 0; i < NCLIENTS; i++) {

            if (pthread_join(threads[i], NULL) != 0) {  //  waiting for termination of threads
                warnx("error joining thread");
                exit(EXIT_FAILURE);
            }
            close(cl_fd[i]);
        }
        count = 0;
    }
    
    close(server_fd);   // Closing server file descriptor

    exit(EXIT_SUCCESS);
}