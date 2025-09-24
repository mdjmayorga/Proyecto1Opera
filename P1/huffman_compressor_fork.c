#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_FILES 100
#define MAX_FILENAME 256
#define MAX_CHARS 256
#define MAX_TREE_HT 256

struct FileInfo {
    char filename[MAX_FILENAME];
    char* content;
    int size;
};

struct FreqMap {
    char character;
    int frequency;
    int used;
};

struct CodeMap {
    char character;
    char code[MAX_TREE_HT];
    int used;
};

struct MinHeapNode {
    char data;
    int freq;
    struct MinHeapNode *left, *right;
};

struct MinHeap {
    int size;
    int capacity;
    struct MinHeapNode** array;
};

struct EncodedDataHeader {
    int encodedLen;   // bits totales
    int byteCount;    // bytes del buffer binario
    int lastBitCount; // # de bits útiles del último byte (1..8)
};

static struct FreqMap freq[MAX_CHARS];
static struct CodeMap codes[MAX_CHARS];
static int freqCount = 0;
static int codeCount = 0;

long long elapsedMillis(struct timeval start, struct timeval end)
{
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;

    if (microseconds < 0) {
        seconds -= 1;
        microseconds += 1000000;
    }

    return seconds * 1000LL + microseconds / 1000LL;
}

static struct MinHeapNode* newNode(char data, int freq)
{
    struct MinHeapNode* node = malloc(sizeof(struct MinHeapNode));
    if (!node) {
        perror("malloc");
        return NULL;
    }
    node->left = node->right = NULL;
    node->data = data;
    node->freq = freq;
    return node;
}

static struct MinHeap* createMinHeap(int capacity)
{
    struct MinHeap* minHeap = malloc(sizeof(struct MinHeap));
    if (!minHeap) {
        perror("malloc");
        return NULL;
    }
    minHeap->size = 0;
    minHeap->capacity = capacity;
    minHeap->array = malloc(minHeap->capacity * sizeof(struct MinHeapNode*));
    if (!minHeap->array) {
        perror("malloc");
        free(minHeap);
        return NULL;
    }
    return minHeap;
}

static void swapMinHeapNode(struct MinHeapNode** a, struct MinHeapNode** b)
{
    struct MinHeapNode* t = *a;
    *a = *b;
    *b = t;
}

static void minHeapify(struct MinHeap* minHeap, int idx)
{
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < minHeap->size &&
        minHeap->array[left]->freq < minHeap->array[smallest]->freq)
        smallest = left;

    if (right < minHeap->size &&
        minHeap->array[right]->freq < minHeap->array[smallest]->freq)
        smallest = right;

    if (smallest != idx) {
        swapMinHeapNode(&minHeap->array[smallest], &minHeap->array[idx]);
        minHeapify(minHeap, smallest);
    }
}

static int isSizeOne(struct MinHeap* minHeap)
{
    return (minHeap->size == 1);
}

static struct MinHeapNode* extractMin(struct MinHeap* minHeap)
{
    struct MinHeapNode* temp = minHeap->array[0];
    minHeap->array[0] = minHeap->array[minHeap->size - 1];
    --minHeap->size;
    minHeapify(minHeap, 0);
    return temp;
}

static void insertMinHeap(struct MinHeap* minHeap, struct MinHeapNode* minHeapNode)
{
    ++minHeap->size;
    int i = minHeap->size - 1;

    while (i && minHeapNode->freq < minHeap->array[(i - 1) / 2]->freq) {
        minHeap->array[i] = minHeap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap->array[i] = minHeapNode;
}

static void storeCodes(struct MinHeapNode* root, char* str, int depth)
{
    if (root == NULL) return;

    
    if (!root->left && !root->right) {
        str[depth] = '\0';
        codes[codeCount].character = root->data;
        strcpy(codes[codeCount].code, str);
        codes[codeCount].used = 1;
        codeCount++;
        return;
    }

    if (root->left) {
        str[depth] = '0';
        storeCodes(root->left, str, depth + 1);
    }
    if (root->right) {
        str[depth] = '1';
        storeCodes(root->right, str, depth + 1);
    }
}

static struct MinHeapNode* buildHuffmanTree()
{
    struct MinHeapNode *left, *right, *top;
    struct MinHeap* minHeap = createMinHeap(freqCount);
    if (!minHeap) return NULL;

    for (int i = 0; i < freqCount; i++) {
        if (freq[i].used) {
            struct MinHeapNode* node = newNode(freq[i].character, freq[i].frequency);
            if (!node) {
                free(minHeap->array);
                free(minHeap);
                return NULL;
            }
            insertMinHeap(minHeap, node);
        }
    }

    while (!isSizeOne(minHeap)) {
        left = extractMin(minHeap);
        right = extractMin(minHeap);

        top = newNode('\0', left->freq + right->freq);
        if (!top) {
            free(minHeap->array);
            free(minHeap);
            return NULL;
        }
        top->left = left;
        top->right = right;

        insertMinHeap(minHeap, top);
    }

    struct MinHeapNode* root = extractMin(minHeap);
    char str[MAX_TREE_HT];
    storeCodes(root, str, 0);

    free(minHeap->array);
    free(minHeap);
    return root;
}

static void calcFreq(char* str, int len)
{
    for (int i = 0; i < len; i++) {
        int found = 0;
        for (int j = 0; j < freqCount; j++) {
            if (freq[j].used && freq[j].character == str[i]) {
                freq[j].frequency++;
                found = 1;
                break;
            }
        }
        if (!found) {
            freq[freqCount].character = str[i];
            freq[freqCount].frequency = 1;
            freq[freqCount].used = 1;
            freqCount++;
        }
    }
}

static const char* getCode(char c)
{
    for (int i = 0; i < codeCount; i++) {
        if (codes[i].used && codes[i].character == c) {
            return codes[i].code;
        }
    }
    return "";
}

static char* readFile(const char* filename, int* size)
{
    FILE* file = fopen(filename, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = malloc(*size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    fread(content, 1, *size, file);
    content[*size] = '\0';

    fclose(file);
    return content;
}

static int readDirectory(const char* dirPath, struct FileInfo* files)
{
    DIR* dir = opendir(dirPath);
    if (!dir) {
        printf("Error: No se pudo abrir el directorio %s\n", dirPath);
        return 0;
    }

    struct dirent* entry;
    int fileCount = 0;
    char fullPath[512];

    while ((entry = readdir(dir)) != NULL && fileCount < MAX_FILES) {
        if (strstr(entry->d_name, ".txt") != NULL) {
            sprintf(fullPath, "%s/%s", dirPath, entry->d_name);

            strcpy(files[fileCount].filename, entry->d_name);
            files[fileCount].content = readFile(fullPath, &files[fileCount].size);

            if (files[fileCount].content != NULL) {
                printf("Archivo leído: %s (%d bytes)\n", entry->d_name, files[fileCount].size);
                fileCount++;
            }
        }
    }

    closedir(dir);
    return fileCount;
}

static ssize_t writeFull(int fd, const void* buffer, size_t count)
{
    size_t total = 0;
    const unsigned char* ptr = buffer;

    while (total < count) {
        ssize_t written = write(fd, ptr + total, count - total);
        if (written < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)written;
    }
    return (ssize_t)total;
}

static ssize_t readFull(int fd, void* buffer, size_t count)
{
    size_t total = 0;
    unsigned char* ptr = buffer;

    while (total < count) {
        ssize_t bytesRead = read(fd, ptr + total, count - total);
        if (bytesRead < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (bytesRead == 0) {
            break;
        }
        total += (size_t)bytesRead;
    }

    return (ssize_t)total;
}

static int encodeFileContent(const struct FileInfo* file, char** encodedContent, int* encodedLen)
{
    int bufferSize = file->size > 0 ? file->size * 20 : 1;
    char* buffer = malloc(bufferSize);
    if (!buffer) {
        perror("malloc");
        return -1;
    }

    int position = 0;
    for (int j = 0; j < file->size; j++) {
        const char* code = getCode(file->content[j]);
        if (code[0] == '\0') {
            fprintf(stderr, "Error: Código no encontrado para caracter %c\n", file->content[j]);
            free(buffer);
            return -1;
        }
        int len = strlen(code);
        if (position + len + 1 >= bufferSize) {
            bufferSize *= 2;
            char* newBuffer = realloc(buffer, bufferSize);
            if (!newBuffer) {
                perror("realloc");
                free(buffer);
                return -1;
            }
            buffer = newBuffer;
        }
        memcpy(buffer + position, code, len);
        position += len;
    }

    buffer[position] = '\0';
    *encodedContent = buffer;
    *encodedLen = position;
    return 0;
}

static int convertEncodedToBuffer(const char* encodedContent, int encodedLen,
                                  unsigned char** binaryBuffer, int* byteCount,
                                  int* lastBitCount)
{
    *byteCount = (encodedLen + 7) / 8;
    int bitsInLastByte = (encodedLen % 8 == 0) ? 8 : (encodedLen % 8);
    if (encodedLen == 0) bitsInLastByte = 8;

    unsigned char* buffer = NULL;
    if (*byteCount > 0) {
        buffer = calloc((size_t)(*byteCount), sizeof(unsigned char));
        if (!buffer) {
            perror("calloc");
            return -1;
        }
    }

    unsigned char currentByte = 0;
    int bitIndex = 0;
    int bufferIndex = 0;

    for (int i = 0; i < encodedLen; i++) {
        currentByte = (currentByte << 1) | (encodedContent[i] - '0');
        bitIndex++;

        if (bitIndex == 8) {
            buffer[bufferIndex++] = currentByte;
            currentByte = 0;
            bitIndex = 0;
        }
    }

    if (bitIndex > 0 && buffer) {
        currentByte <<= (8 - bitIndex);
        buffer[bufferIndex++] = currentByte;
    }

    *binaryBuffer = buffer;
    *lastBitCount = bitsInLastByte;
    return 0;
}
int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("Uso: %s <directorio_entrada> <archivo_salida.bin>\n", argv[0]);
        return 1;
    }

    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);

    struct FileInfo files[MAX_FILES];
    long totalSize = 0;

    memset(freq, 0, sizeof(freq));
    memset(codes, 0, sizeof(codes));
    freqCount = 0;
    codeCount = 0;

    int fileCount = readDirectory(argv[1], files);
    if (fileCount == 0) {
        printf("No se encontraron archivos .txt en el directorio\n");
        return 1;
    }

    for (int i = 0; i < fileCount; i++) {
        calcFreq(files[i].content, files[i].size);
        totalSize += files[i].size;
    }

    printf("\nCalculando frecuencias de %ld caracteres...\n", totalSize);

    printf("Construyendo árbol de Huffman...\n");
    struct MinHeapNode* root = buildHuffmanTree();
    if (!root) {
        fprintf(stderr, "Error construyendo el árbol de Huffman\n");
        return 1;
    }

    FILE* outFile = fopen(argv[2], "wb");
    if (!outFile) {
        printf("Error: No se pudo crear el archivo de salida\n");
        return 1;
    }

    fwrite(&fileCount, sizeof(int), 1, outFile);
    fwrite(&codeCount, sizeof(int), 1, outFile);

    for (int i = 0; i < codeCount; i++) {
        if (codes[i].used) {
            fwrite(&codes[i].character, sizeof(char), 1, outFile);
            int codeLen = strlen(codes[i].code);
            fwrite(&codeLen, sizeof(int), 1, outFile);
            fwrite(codes[i].code, sizeof(char), codeLen, outFile);
        }
    }

    // Procesar cada archivo con un hijo
    for (int i = 0; i < fileCount; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            fclose(outFile);
            return 1;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(pipefd[0]);
            close(pipefd[1]);
            fclose(outFile);
            return 1;
        }

        if (pid == 0) {
            close(pipefd[0]);

            char* encodedContent = NULL;
            int encodedLen = 0;
            if (encodeFileContent(&files[i], &encodedContent, &encodedLen) != 0) {
                close(pipefd[1]);
                _exit(1);
            }

            unsigned char* binaryBuffer = NULL;
            int byteCount = 0;
            int lastBitCount = 8;
            if (convertEncodedToBuffer(encodedContent, encodedLen, &binaryBuffer,
                                        &byteCount, &lastBitCount) != 0) {
                free(encodedContent);
                close(pipefd[1]);
                _exit(1);
            }

            struct EncodedDataHeader header = { encodedLen, byteCount, lastBitCount };
            writeFull(pipefd[1], &header, sizeof(header));

            if (byteCount > 0)
                writeFull(pipefd[1], binaryBuffer, (size_t)byteCount);

            free(binaryBuffer);
            free(encodedContent);
            close(pipefd[1]);
            _exit(0);
        } else { 
            close(pipefd[1]);

            int nameLen = strlen(files[i].filename);
            fwrite(&nameLen, sizeof(int), 1, outFile);
            fwrite(files[i].filename, sizeof(char), nameLen, outFile);

            struct EncodedDataHeader header;
            if (readFull(pipefd[0], &header, sizeof(header)) != sizeof(header)) {
                fprintf(stderr, "Error leyendo datos codificados del hijo\n");
                close(pipefd[0]);
                fclose(outFile);
                return 1;
            }
            ..f.f.

            fwrite(&header.encodedLen, sizeof(int), 1, outFile);

            if (header.byteCount > 0) {
                unsigned char* buffer = malloc((size_t)header.byteCount);
                if (!buffer) {
                    perror("malloc");
                    close(pipefd[0]);
                    fclose(outFile);
                    return 1;
                }
                if (readFull(pipefd[0], buffer, (size_t)header.byteCount) != header.byteCount) {
                    fprintf(stderr, "Error leyendo datos binarios del hijo\n");
                    free(buffer);
                    close(pipefd[0]);
                    fclose(outFile);
                    return 1;
                }
                fwrite(buffer, 1, (size_t)header.byteCount, outFile);
                free(buffer);
            }

            fwrite(&header.lastBitCount, sizeof(int), 1, outFile);

            close(pipefd[0]);
            waitpid(pid, NULL, 0);

            printf("Archivo %s codificado mediante PID %d\n", files[i].filename, pid);
            free(files[i].content);
        }
    }

    fclose(outFile);

    printf("\nCompresión completada: %s\n", argv[2]);
    gettimeofday(&endTime, NULL);
    long long totalMs = elapsedMillis(startTime, endTime);
    printf("Tiempo total de compresión: %lld ms\n", totalMs);

    return 0;
}
