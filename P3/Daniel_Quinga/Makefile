CC=gcc
DEB=-DDEBUG
DEPS = proxy.h
CFLAGS= -lpthread
OBJ_SERVER = proxy.o server.o 
OBJ_CLIENT = proxy.o client.o

%.o: %.c $(DEPS)
	$(CC) -Wshadow -Wall -c -o $@ $< $(CFLAGS)

practica_3: server client

server: $(OBJ_SERVER)
	gcc -o server $(OBJ_SERVER) $(CFLAGS)

client: $(OBJ_CLIENT)
	gcc -o client $(OBJ_CLIENT) $(CFLAGS)

clean:
	rm *.o
	rm server client 