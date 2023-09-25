CC=gcc
DEPS=-lpthread
DEB=-DDEBUG

practica_2: server client

server: server.c
	$(CC) -o server server.c $(DEPS)

client: client.c
	$(CC) -o client client.c

debug_server: clean_s server.c
	$(CC) -o server server.c $(DEPS) $(DEB)

debug_client: clean_c client.c
	$(CC) -o client client.c $(DEB)

clean_s:
	rm server

clean_c:
	rm client