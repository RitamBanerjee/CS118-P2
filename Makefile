CC=gcc
CFLAGS=-I.
DEPS = # header file

%.o: %.c $(DEPS)
	$(CC) -c -o  $@ $< $(CFLAGS)

server: udpserver.o
	$(CC) -o $@ $^ $(CFLAGS)

client: udpclient.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	\rm -f *.o client server