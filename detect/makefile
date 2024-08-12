CC = gcc
CFLAGS = -Wall
LIBS = -lcurl -lpthread

all: detect

detect: detect.o
	$(CC) $(CFLAGS) -o detect detect.o $(LIBS)

detect.o: detect.c
	$(CC) $(CFLAGS) -c detect.c

clean:
	rm -f exe detect.o