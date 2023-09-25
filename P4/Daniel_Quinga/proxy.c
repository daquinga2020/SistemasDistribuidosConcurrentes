#include "proxy.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <err.h>
#include <errno.h>

int serv_fd;

int init_serv(char *ip, int port) {

    struct sockaddr_in servaddr;
    const int enable = 1;
    
    // Creating socket file descriptor
    serv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_fd < 0) {
        perror("socket failed");
        return -1;
    }
    
    // Assign IP, SERV_PORT, IPV4 to server
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);

    //  To reuse the same port
    if (setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt failed");
        return -1;
    }

    // Assigning a name to a socket(IP => port)
    if (bind(serv_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        return -1;
    }

    // Specifying a willingness to accept incoming connections 
    // and a queue limit for incoming connections
    if (listen(serv_fd, 2000) < 0) { // Cambiar backlog
        perror("listen failed");
        return -1;
    }

    return 0;
}

// Funcion servidor
void wait_cl(int *serv_cl_fd) {

    socklen_t addrlen;
    struct sockaddr sclient;
    
    addrlen = sizeof(sclient);

    *serv_cl_fd = accept(serv_fd, (struct sockaddr *) &sclient, &addrlen);
    
    if (*serv_cl_fd < 0) {
        err(EXIT_FAILURE, "accept failed");
    }
}

// Funcion servidor 
int receive_msg(int fd, struct message *msg, int flag) {
    
    if (recv(fd, msg, sizeof(struct message), flag) < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv failed");
        }
        return -1;
    }
    
    return 0;
}

// Funcion servidor
void send_resp(int fd, struct response resp) {

    if (send(fd, &resp, sizeof(resp), 0) < 0) {
        err(EXIT_FAILURE,"send failed");
    }
}

// Funcion cliente
void connect2serv(char *ip, int port, int *cl_proc_fd) {

    struct sockaddr_in claddr;
    int err_connect;

    // Creating socket file descriptor
    *cl_proc_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*cl_proc_fd < 0) {
        err(EXIT_FAILURE, "socket failed");
    }
    
    // Assign IP, SERV_PORT, IPV4 to client
    claddr.sin_family = AF_INET;
    //  Put IP of localhost
    claddr.sin_addr.s_addr = inet_addr(ip);
    claddr.sin_port = htons(port);

    // Connecting to the server
    err_connect = connect(*cl_proc_fd, (struct sockaddr *)&claddr, sizeof(claddr));
    if (err_connect < 0) {
        err(EXIT_FAILURE, "connect failed");
    }
}

// Funcion cliente
void send_msg(int fd, struct message msg) {

    if (send(fd, &msg, sizeof(msg), 0) < 0) {
        err(EXIT_FAILURE,"send failed");
    }
}

// Funcion cliente
void receive_resp(int fd, struct response *resp) {

    if (recv(fd, resp, sizeof(struct response), 0) < 0) {
        err(EXIT_FAILURE,"recv failed");
    }
}