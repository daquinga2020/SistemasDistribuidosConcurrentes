CC=gcc
DEB=-DDEBUG
DEPS = proxy.h
CFLAGS= -lpthread
OBJ_PUBLISHER = proxy.o publisher.o 
OBJ_SUBSCRIBER = proxy.o subscriber.o
OBJ_BROKER = proxy.o broker.o

%.o: %.c $(DEPS)
	$(CC) -Wshadow -Wall -c -o $@ $< $(CFLAGS)

practica_2: publisher subscriber broker

publisher: $(OBJ_PUBLISHER)
	gcc -o publisher $(OBJ_PUBLISHER) $(CFLAGS)

subscriber: $(OBJ_SUBSCRIBER)
	gcc -o subscriber $(OBJ_SUBSCRIBER) $(CFLAGS)

broker: $(OBJ_BROKER)
	gcc -o broker $(OBJ_BROKER) $(CFLAGS)

clean:
	rm *.o
	rm publisher subscriber broker