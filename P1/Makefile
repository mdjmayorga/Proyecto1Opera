CC = gcc
CFLAGS = -Wall -Wextra -g

all: huffman_compressor huffman_decompressor uffman_compressor_fork huffman_decompressor_fork

huffman_compressor: huffman_compressor.c
	$(CC) $(CFLAGS) -o huffman_compressor huffman_compressor.c

huffman_decompressor: huffman_decompressor.c
	$(CC) $(CFLAGS) -o huffman_decompressor huffman_decompressor.c

huffman_compressor_fork: huffman_compressor_fork.c
	$(CC) $(CFLAGS) -o huffman_compressor_fork huffman_compressor_fork.c

huffman_decompressor_fork: huffman_decompressor_fork.c
	$(CC) $(CFLAGS) -o huffman_decompressor_fork huffman_decompressor_fork.c

clean:
	rm -f huffman_compressor huffman_decompressor huffman_compressor_fork huffman_decompressor_fork

.PHONY: all clean