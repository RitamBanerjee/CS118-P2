CC=gcc
CFLAGS=-I.
DEPS = # header file


all: udpserver.o udpclient.o
	$(CC) -o client udpclient.c $(CFLAGS)
	$(CC) -o server udpserver.c $(CFLAGS)

server: udpserver.o
	$(CC) -o $@ $^ $(CFLAGS)

client: udpclient.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	\rm -f *.o client server