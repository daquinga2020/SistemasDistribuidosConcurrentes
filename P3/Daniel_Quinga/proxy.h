enum operations {
    WRITE = 0,
    READ
};

struct request {
    enum operations action; // Contiene valores de operations
    unsigned int id;    // Contiene la id del cliente que realiza la peticion
};

struct response {
    enum operations action; // Contiene valores de operations
    unsigned int counter;   // Valor del contador
    long waiting_time;  // Numero de nanosegundos que ha esperado el cliente para llevar a cabo su operacion(lectura o escritura)
};

// Start the server socket
int init_serv(char *ip, int port);

// Wait for a client
void wait_cl(int *serv_cl_fd);

void receiving_rqs(int fd, struct request *rq);

void receiving_rps(int fd, struct response *rp);

// Client connects to server
void connect2serv(char *ip, int port, int *cl_proc_fd);

void send_request(int fd, struct request rq);

void send_response(int fd, struct response rp);