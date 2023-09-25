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

// Client connects to server
void connect2serv();

// Wait for a client
void wait_cl(int *serv_cl_fd);

void receiving_msgs(int fd, struct request *rq);

// Client connects to server
void connect2serv(char *ip, int port, int *cl_proc_fd);

void send_rq(int fd, struct request *rq);

// Close client
void close_cl();

// Close server
void close_serv();

/*
// Set the process name
void set_name (char name[2]);

// Set ip and puerto
void set_ip_port (char* ip, unsigned int port);

// Notifies that it is ready to perform shutdown (READY_TO_SHUTDOWN)
void notify_ready_shutdown();

// Notifies that it is going to perform the shutdown correctly (SHUTDOWN_ACK)
void notify_shutdown_ack();

// Order the shutdown (SHUTDOWN_NOW)
void notify_shutdown_now();

// Wait for a message from a client
void wait_msg2client();

// Wait for a message from the server
void wait_msg2serv();

// Returns the value of the lamport clock
int get_clock_lamport();

*/