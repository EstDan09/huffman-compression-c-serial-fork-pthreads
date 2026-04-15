CC = gcc
CFLAGS = -Wall -O2

all: compress decompress

compress: compress.c huffman.c huffman.h
	$(CC) $(CFLAGS) -o compress compress.c huffman.c -lpthread

decompress: decompress.c huffman.c huffman.h
	$(CC) $(CFLAGS) -o decompress decompress.c huffman.c -lpthread

clean:
	rm -f compress decompress *.o
