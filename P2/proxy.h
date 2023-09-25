enum operations {
    READY_TO_SHUTDOWN = 0,  // Machine notifies that it is ready to shut down
    SHUTDOWN_NOW,   // Machine knows to turn off
    SHUTDOWN_ACK    // Machine sends this message just before performing the shutdown
};

struct message {
    char origin[20];    // Name of the process sending the message
    enum operations action; // Contains values from the operations enumeration
    unsigned int clock_lamport; // Send lamport counter
};

// Set the process name
void set_name (char name[2]);

// Start the server socket
int init_serv();

// Wait for a client
void wait_cl();

// Set ip and puerto
void set_ip_port (char* ip, unsigned int port);

// Client connects to server
void connect2serv();

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

// Close client
void close_cl();

// Close server
void close_serv();