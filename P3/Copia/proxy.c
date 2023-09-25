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

#define MAX_CLIENTS 5

//int serv_proc_fds[MAX_CLIENTS];
int serv_fd;    
int nthreads = 0;
//pthread_t thrds[MAX_CLIENTS];

// Funcion servidor
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
    if (listen(serv_fd, 100) < 0) {
        perror("listen failed");
        return -1;
    }

    return 0;
}

// Callback de funcion servidor wait_cl
void * init_comunication(void *id) {

    int *p_id = id;

    //recv_msgs(serv_proc_fds[nthreads-1], nthreads-1, "READY_TO_SHUTDOWN");
    printf("HILO #%d#\n", *p_id);
    //nthreads--;

    pthread_exit(NULL);
}

// Funcion servidor
void wait_cl(int *serv_cl_fd){

    socklen_t addrlen;
    struct sockaddr sclient;
    
    addrlen = sizeof(sclient);

    *serv_cl_fd = accept(serv_fd, (struct sockaddr *) &sclient, &addrlen);
    
    if (*serv_cl_fd < 0) {
        err(EXIT_FAILURE, "accept failed");
    }
    /*if (nthreads >= MAX_CLIENTS) {
        nthreads = 0;
    }*/

    /*serv_proc_fds[nthreads] = accept(serv_fd, (struct sockaddr *) &sclient, &addrlen);
    if (serv_proc_fds[nthreads] < 0) {
        err(EXIT_FAILURE, "accept failed");
    }*/

    /*if (pthread_create(&thrds[nthreads], NULL, init_comunication, &nthreads) != 0) {
        warnx("error creating thread");
        exit(EXIT_FAILURE);
    }*/
    //nthreads++;
}

// funcion Server
void receiving_msgs(int fd, struct request *rq) {

    if (recv(fd, rq, sizeof(rq), 0) < 0) {
        perror("recv failed");
    }
    printf("RECIBI MENSAJE\n");
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

void send_rq(int fd, struct request *rq) {
    printf("MENSAJE DE ESCRITOR A SERVIDOR\n");

    if (send(fd, rq, sizeof(rq), 0) < 0) {
        err(EXIT_FAILURE,"send failed");
    }
}

void reader_mode() {
    printf("MODO LECTOR");
}

void close_cl() {

    //close(cl_proc_fd);
}

/*void close_serv() {

    int i;

    for (i = 0; i < nthreads; i++) {
        close(serv_proc_fds[i]);
    }
    close(serv_fd);
}*/

/*#define TXT 64
#define MAX_CLIENTS 2

struct message msg;
static int lamport_clock = 0;

int sf_port;
char* sf_ip;

char names_cls[MAX_CLIENTS][TXT];

int serv_proc_fds[MAX_CLIENTS];
int serv_fd;
int cl_proc_fd;

int nthreads = 0;
pthread_t thrds[MAX_CLIENTS];


void set_name (char name[2]) {

    strncpy(msg.origin, name, 2);
}

int init_serv() {

    struct sockaddr_in servaddr;
    const int enable = 1;
    msg.clock_lamport = 0;
    
    // Creating socket file descriptor
    serv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_fd < 0) {
        perror("socket failed");
        return -1;
    }
    
    // Assign IP, SERV_PORT, IPV4 to server
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(sf_ip);
    servaddr.sin_port = htons(sf_port);

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
    if (listen(serv_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        return -1;
    }
    
    return 0;
}

void recv_msgs(int fd, int ind, char *action) {

    struct message proc_msg;

    if (recv(fd, &proc_msg, sizeof(proc_msg), 0) < 0) {
        perror("recv failed");
        pthread_exit(NULL);
    }
    
    if (get_clock_lamport() < proc_msg.clock_lamport) {
        lamport_clock = proc_msg.clock_lamport;
    }
    
    lamport_clock++;
    
    msg.clock_lamport = get_clock_lamport();
    strncpy(names_cls[ind], proc_msg.origin, sizeof(proc_msg.origin));

    printf("%s, %d, RECV(%s), %s\n", msg.origin, get_clock_lamport(), proc_msg.origin, action);
    
    pthread_exit(NULL);
}

void *
init_comunication() {

    recv_msgs(serv_proc_fds[nthreads-1], nthreads-1, "READY_TO_SHUTDOWN");

    pthread_exit(NULL);
}

void wait_cl(){

    socklen_t addrlen;
    struct sockaddr sclient;
    
    addrlen = sizeof(sclient);

    serv_proc_fds[nthreads] = accept(serv_fd, (struct sockaddr *) &sclient, &addrlen);
    if (serv_proc_fds[nthreads] < 0) {
        err(EXIT_FAILURE, "accept failed");
    }
    
    if (pthread_create(&thrds[nthreads], NULL, init_comunication, NULL) != 0) {
        warnx("error creating thread");
        exit(EXIT_FAILURE);
    }
    nthreads++;
}

void set_ip_port (char* ip, unsigned int port) {

    sf_ip = ip;
    sf_port = port;
}

void connect2serv() {

    struct sockaddr_in claddr;
    int err_connect;
    msg.clock_lamport = 0;

    // Creating socket file descriptor
    cl_proc_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cl_proc_fd < 0) {
        err(EXIT_FAILURE, "socket failed");
    }
    
    // Assign IP, SERV_PORT, IPV4 to client
    claddr.sin_family = AF_INET;
    //  Put IP of localhost
    claddr.sin_addr.s_addr = inet_addr(sf_ip);
    claddr.sin_port = htons(sf_port);

    // Connecting to the server
    err_connect = connect(cl_proc_fd, (struct sockaddr *)&claddr, sizeof(claddr));
    if (err_connect < 0) {
        err(EXIT_FAILURE, "connect failed");
    }
}

void send_msg(int fd, char *action) {

    lamport_clock++;
    msg.clock_lamport = get_clock_lamport();
    msg.action = READY_TO_SHUTDOWN;
    
    printf("%s, %d, SEND, %s\n", msg.origin, msg.clock_lamport, action);
    
    if (send(fd, &msg, sizeof(msg), 0) < 0) {
        err(EXIT_FAILURE,"send failed");
    }
}

void notify_ready_shutdown() {

    // Sending msg: it's ready power off (READY_TO_SHUTDOWN)
    send_msg(cl_proc_fd, "READY_TO_SHUTDOWN");
}

void notify_shutdown_ack() {

    // Sending msg: it will power off (SHUTDOWN_ACK)
    send_msg(cl_proc_fd, "SHUTDOWN_ACK");
}

void find_cl(char *name, int *ind) {

    int i;
    for(i = 0; i < nthreads; i++) {
        if (names_cls[i][1] == name[1]) {
            *ind = i;
        }
    }
}

void notify_shutdown_now() {

    int id, i;

    if (get_clock_lamport() < 3) {
        for(i = 0; i < nthreads; i++) {
            pthread_join(thrds[i], NULL);
        }    
    }
    
    if (get_clock_lamport() == 3) {
        find_cl("P1", &id);
    } else if (get_clock_lamport() == 7){
        find_cl("P3",&id);
    } else {
        errx(EXIT_FAILURE, "poorly timed communication");
    }
    
    // Sending msg: order to power off (SHUTDOWN_NOW)
    send_msg(serv_proc_fds[id], "SHUTDOWN_NOW");
}

void *
recv_msg2clt(void *ind) {

    int *p_ind = ind;

    recv_msgs(serv_proc_fds[*p_ind], *p_ind, "SHUTDOWN_ACK");

    pthread_exit(NULL);
}

void wait_msg2client() {

    int ind;
    
    if (get_clock_lamport() == 4) {
        find_cl("P1",&ind);
    } else if (get_clock_lamport() == 8){
        find_cl("P3",&ind);
    } else {
        errx(EXIT_FAILURE, "poorly timed communication");
    }

    if (pthread_create(&thrds[0], NULL, recv_msg2clt, &ind) != 0) {
        warnx("error creating thread");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_join(thrds[0], NULL) != 0) {
        warnx("error joining thread");
        exit(EXIT_FAILURE);
    }
}

void *
recv_msg2serv() {

    recv_msgs(cl_proc_fd, 0, "SHUTDOWN_NOW");
    
    pthread_exit(NULL);
}

void wait_msg2serv() {
    
    
    if (pthread_create(&thrds[0], NULL, recv_msg2serv, NULL) != 0) {
        warnx("error creating thread");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_join(thrds[0], NULL) != 0) {
        warnx("error joining thread");
        exit(EXIT_FAILURE);
    }
}

int get_clock_lamport() {

    return lamport_clock;
}

void close_cl() {

    close(cl_proc_fd);
}

void close_serv() {

    int i;

    for (i = 0; i < nthreads; i++) {
        close(serv_proc_fds[i]);
    }
    close(serv_fd);
}
*/