#include <stdio.h>
#include <stdlib.h>
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
#define PORT 8080

typedef void (*sighandler_t)(int);

int client_fd;

void
handler_cl (int number) {
    close(client_fd);
    exit(EXIT_SUCCESS);
}

int main() {

    int err_connect;
    char msg_cl[MAX_LINE], msg_sv[MAX_LINE];
    char *IPbuffer;
    struct sockaddr_in claddr;
    
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
    claddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    claddr.sin_port = htons(PORT);

    // Connecting to the server
    err_connect = connect(client_fd, (struct sockaddr *)&claddr, sizeof(claddr));
    if (err_connect < 0) {
        err(EXIT_FAILURE, "connect failed");
    }
    
    printf("conected to the server...\n");

    signal(SIGINT, handler_cl);
    
    // Write/read to/from server
    while(1) {

        // Sending msg to server
        printf("> ");
        fgets(msg_cl, MAX_LINE, stdin);
        if (send(client_fd, msg_cl, sizeof(msg_cl), 0) < 0) {
            perror("send failed");
            break;
        }
        
        //Waiting msg from server
        if (recv(client_fd, msg_sv, sizeof(msg_sv), 0) < 0) {
            perror("recv failed");
            break;
        }
        printf("+++ %s", msg_sv);
    }

    // Close client
    close(client_fd); 
    exit(EXIT_FAILURE);
}