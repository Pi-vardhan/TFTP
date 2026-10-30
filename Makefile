CC = gcc
CFLAGS = -Wall -Wextra

COMMON = udp.c tftp.c file.c

.PHONY: all clean

all: client server

client: client.c $(COMMON)
	$(CC) $(CFLAGS) client.c $(COMMON) -o client

server: server.c $(COMMON)
	$(CC) $(CFLAGS) server.c $(COMMON) -o server

clean:
	rm -f client server
