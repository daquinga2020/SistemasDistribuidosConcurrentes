CC=gcc
DEB=-DDEBUG
DEPS = proxy.h
CFLAGS= -lpthread
OBJ_P1 = proxy.o P1.o 
OBJ_P2 = proxy.o P2.o
OBJ_P3 = proxy.o P3.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

practica_2: lamport_P1 lamport_P2 lamport_P3

lamport_P1: $(OBJ_P1)
	gcc -o P1 $(OBJ_P1) $(CFLAGS)

lamport_P2: $(OBJ_P2)
	gcc -o P2 $(OBJ_P2) $(CFLAGS)

lamport_P3: $(OBJ_P3)
	gcc -o P3 $(OBJ_P3) $(CFLAGS)

clean:
	rm *.o
	rm P1 P2 P3