#include <time.h>

enum operations {
    REGISTER_PUBLISHER = 0,
    UNREGISTER_PUBLISHER,
    REGISTER_SUBSCRIBER,
    UNREGISTER_SUBSCRIBER,
    PUBLISH_DATA
};

struct publish {
    struct timespec time_generated_data;
    char data[100];
};

struct message {
    enum operations action;
    char topic[100];
    // Solo utilizado en mensajes de UNREGISTER
    int id;
    // Solo utilizado en mensajes PUBLISH_DATA
    struct publish data;
};

enum status {
    ERROR = 0,
    LIMIT,
    OK
};

struct response {
    enum status response_status;
    int id;
};

int init_serv(char *ip, int port);

void wait_cl(int *serv_cl_fd);

int receive_msg(int fd, struct message *msg, int flag);

void send_resp(int fd, struct response resp);

void connect2serv(char *ip, int port, int *cl_proc_fd);

void send_msg(int fd, struct message msg);

void receive_resp(int fd, struct response *resp);