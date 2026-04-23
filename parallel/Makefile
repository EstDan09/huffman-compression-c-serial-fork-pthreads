CC = gcc
CFLAGS = -Wall -O2 -Wno-format-truncation

all: compress decompress

compress: compress.c huffman.c huffman.h
	$(CC) $(CFLAGS) -o compress compress.c huffman.c

decompress: decompress.c huffman.c huffman.h
	$(CC) $(CFLAGS) -o decompress decompress.c huffman.c

clean:
	rm -f compress decompress *.o
