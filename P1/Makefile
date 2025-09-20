CC = gcc
CFLAGS = -Wall -Wextra -g

all: huffman_compressor huffman_decompressor

huffman_compressor: huffman_compressor.c
	$(CC) $(CFLAGS) -o huffman_compressor huffman_compressor.c

huffman_decompressor: huffman_decompressor.c
	$(CC) $(CFLAGS) -o huffman_decompressor huffman_decompressor.c

clean:
	rm -f huffman_compressor huffman_decompressor

.PHONY: all clean