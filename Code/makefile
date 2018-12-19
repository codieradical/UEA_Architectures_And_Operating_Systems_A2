CC=gcc 
CFLAGS=-Wall

all: 
	mkdir -p bin
	$(CC) listfiles.c -o bin/listfiles $(CFLAGS)
	$(CC) backupfiles.c -o bin/backupfiles $(CFLAGS)

clean:
	rm -rf bin