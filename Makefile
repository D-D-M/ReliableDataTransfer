CC=gcc
CFLAGS=-Wall -I.
DEPS = # header file
OBJ = server.o client.o

%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

all: server client

server: server.o
	$(CC) $(CFLAGS) -o $@ $< 

client: client.o
	$(CC) $(CFLAGS) -o $@ $< 

clean:
	rm -f server server.o client client.o
