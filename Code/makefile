CC=gcc 
CFLAGS=-Wall

all: 
	mkdir -p bin
	$(CC) listfiles.c -o bin/listfiles $(CFLAGS)
	$(CC) backupfiles.c -o bin/backupfiles $(CFLAGS)
	$(CC) backup.c -o bin/backup $(CFLAGS)

clean:
	rm -rf bin *.tar
	find . -name "*.tar*" -type f -delete