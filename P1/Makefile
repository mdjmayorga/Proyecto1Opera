CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

all: huffman_compressor huffman_decompressor huffman_compressor_fork huffman_decompressor_fork huffman_compressor_pthread huffman_decompressor_pthread

huffman_compressor: huffman_compressor.c
	$(CC) $(CFLAGS) -o huffman_compressor huffman_compressor.c

huffman_decompressor: huffman_decompressor.c
	$(CC) $(CFLAGS) -o huffman_decompressor huffman_decompressor.c

huffman_compressor_fork: huffman_compressor_fork.c
	$(CC) $(CFLAGS) -o huffman_compressor_fork huffman_compressor_fork.c

huffman_decompressor_fork: huffman_decompressor_fork.c
	$(CC) $(CFLAGS) -o huffman_decompressor_fork huffman_decompressor_fork.c

huffman_compressor_pthread: huffman_compressor_pthread.c
	$(CC) $(CFLAGS) -o huffman_compressor_pthread huffman_compressor_pthread.c

huffman_decompressor_pthread: huffman_decompressor_pthread.c
	$(CC) $(CFLAGS) -o huffman_decompressor_pthread huffman_decompressor_pthread.c

clean:
	rm -f huffman_compressor huffman_decompressor huffman_compressor_fork huffman_decompressor_fork huffman_compressor_pthread huffman_decompressor_pthread

.PHONY: all clean