#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_CHARS 256
#define MAX_TREE_HT 256

struct MinHeapNode {
    char data;
    struct MinHeapNode *left, *right;
};

struct CodeInfo {
    char character;
    char code[MAX_TREE_HT];
};

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

static struct MinHeapNode* newNode(char data)
{
    struct MinHeapNode* node = malloc(sizeof(struct MinHeapNode));
    if (!node) {
        perror("malloc");
        return NULL;
    }
    node->left = node->right = NULL;
    node->data = data;
    return node;
}

static struct MinHeapNode* buildTreeFromCodes(struct CodeInfo* codes, int codeCount)
{
    struct MinHeapNode* root = newNode('$');
    if (!root) return NULL;

    for (int i = 0; i < codeCount; i++) {
        struct MinHeapNode* current = root;
        char* code = codes[i].code;

        for (int j = 0; code[j] != '\0'; j++) {
            if (code[j] == '0') {
                if (current->left == NULL) {
                    current->left = newNode('$');
                    if (!current->left) return root;
                }
                current = current->left;
            } else {
                if (current->right == NULL) {
                    current->right = newNode('$');
                    if (!current->right) return root;
                }
                current = current->right;
            }
        }
        current->data = codes[i].character;
    }

    return root;
}

static char* bytesToBinaryString(const unsigned char* bytes, int byteCount,
                                 int encodedLen, int lastBitCount)
{
    if (encodedLen <= 0) {
        char* empty = malloc(1);
        if (!empty) {
            perror("malloc");
            return NULL;
        }
        empty[0] = '\0';
        return empty;
    }

    char* binStr = malloc((size_t)encodedLen + 1);
    if (!binStr) {
        perror("malloc");
        return NULL;
    }

    int bitIndex = 0;
    for (int b = 0; b < byteCount && bitIndex < encodedLen; b++) {
        unsigned char byte = bytes[b];
        int bitsToRead = 8;
        if (b == byteCount - 1) {
            bitsToRead = (encodedLen % 8 == 0) ? 8 : lastBitCount;
        }

        for (int bit = 7; bit >= 8 - bitsToRead && bitIndex < encodedLen; bit--) {
            binStr[bitIndex++] = ((byte >> bit) & 1) ? '1' : '0';
        }
    }

    binStr[bitIndex] = '\0';
    return binStr;
}

static char* decode_file(struct MinHeapNode* root, char* s)
{
    if (!root || !s) return NULL;

    int len = strlen(s);
    char* ans = malloc((size_t)len * 2 + 1);
    if (!ans) {
        perror("malloc");
        return NULL;
    }
    struct MinHeapNode* curr = root;
    int ansIndex = 0;

    for (int i = 0; i < len; i++) {
        if (s[i] == '0') {
            if (curr->left) curr = curr->left;
        } else if (s[i] == '1') {
            if (curr->right) curr = curr->right;
        }

        if (curr && curr->left == NULL && curr->right == NULL && curr->data != '$') {
            ans[ansIndex++] = curr->data;
            curr = root;
        }

        if (!curr) {
            printf("Error: Árbol corrupto en posición %d\n", i);
            free(ans);
            return NULL;
        }
    }
    ans[ansIndex] = '\0';
    return ans;
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("Uso: %s <archivo_comprimido.bin> <directorio_salida>\n", argv[0]);
        return 1;
    }

    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);

    FILE* inFile = fopen(argv[1], "rb");
    if (!inFile) {
        printf("Error: No se pudo abrir el archivo %s\n", argv[1]);
        return 1;
    }

    mkdir(argv[2], 0755);

    int fileCount, codeCount;
    if (fread(&fileCount, sizeof(int), 1, inFile) != 1 ||
        fread(&codeCount, sizeof(int), 1, inFile) != 1) {
        printf("Error: No se pudo leer la cabecera\n");
        fclose(inFile);
        return 1;
    }

    printf("Archivos a descomprimir: %d\n", fileCount);
    printf("Códigos en tabla: %d\n", codeCount);

    if (codeCount <= 0 || codeCount > MAX_CHARS) {
        printf("Error: Número de códigos inválido: %d\n", codeCount);
        fclose(inFile);
        return 1;
    }

    struct CodeInfo* codes = malloc((size_t)codeCount * sizeof(struct CodeInfo));
    if (!codes) {
        perror("malloc");
        fclose(inFile);
        return 1;
    }

    for (int i = 0; i < codeCount; i++) {
        if (fread(&codes[i].character, sizeof(char), 1, inFile) != 1) {
            printf("Error leyendo carácter %d\n", i);
            free(codes);
            fclose(inFile);
            return 1;
        }

        int codeLen;
        if (fread(&codeLen, sizeof(int), 1, inFile) != 1) {
            printf("Error leyendo longitud de código %d\n", i);
            free(codes);
            fclose(inFile);
            return 1;
        }

        if (codeLen <= 0 || codeLen >= MAX_TREE_HT) {
            printf("Error: Longitud de código inválida: %d\n", codeLen);
            free(codes);
            fclose(inFile);
            return 1;
        }

        if (fread(codes[i].code, sizeof(char), codeLen, inFile) != (size_t)codeLen) {
            printf("Error leyendo código %d\n", i);
            free(codes);
            fclose(inFile);
            return 1;
        }
        codes[i].code[codeLen] = '\0';

        printf("Código: '%c' -> %s\n", codes[i].character, codes[i].code);
    }

    struct MinHeapNode* root = buildTreeFromCodes(codes, codeCount);
    if (!root) {
        fprintf(stderr, "Error al reconstruir el árbol de Huffman\n");
        free(codes);
        fclose(inFile);
        return 1;
    }

    for (int i = 0; i < fileCount; i++) {
        printf("\nProcesando archivo %d/%d...\n", i + 1, fileCount);

        int nameLen;
        if (fread(&nameLen, sizeof(int), 1, inFile) != 1) {
            printf("Error leyendo longitud del nombre\n");
            break;
        }

        if (nameLen <= 0 || nameLen > 1000) {
            printf("Error: Longitud de nombre inválida: %d\n", nameLen);
            break;
        }

        char* filename = malloc((size_t)nameLen + 1);
        if (!filename) {
            perror("malloc");
            break;
        }

        if (fread(filename, sizeof(char), nameLen, inFile) != (size_t)nameLen) {
            printf("Error leyendo nombre del archivo\n");
            free(filename);
            break;
        }
        filename[nameLen] = '\0';

        int encodedLen;
        if (fread(&encodedLen, sizeof(int), 1, inFile) != 1) {
            printf("Error leyendo longitud codificada\n");
            free(filename);
            break;
        }

        printf("Archivo: %s, bits codificados: %d\n", filename, encodedLen);

        int byteCount = (encodedLen + 7) / 8;
        unsigned char* bytes = NULL;
        if (byteCount > 0) {
            bytes = malloc((size_t)byteCount);
            if (!bytes) {
                perror("malloc");
                free(filename);
                break;
            }

            if (fread(bytes, 1, (size_t)byteCount, inFile) != (size_t)byteCount) {
                printf("Error leyendo datos binarios\n");
                free(filename);
                free(bytes);
                break;
            }
        }

        int lastBitCount;
        if (fread(&lastBitCount, sizeof(int), 1, inFile) != 1) {
            printf("Error leyendo lastBitCount\n");
            free(filename);
            free(bytes);
            break;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            free(filename);
            free(bytes);
            break;
        }

        if (pid == 0) {
            char* binaryStr = bytesToBinaryString(bytes, byteCount, encodedLen, lastBitCount);
            if (!binaryStr) {
                free(filename);
                free(bytes);
                _exit(1);
            }

            char* decodedContent = decode_file(root, binaryStr);
            if (!decodedContent) {
                free(binaryStr);
                free(filename);
                free(bytes);
                _exit(1);
            }

            char outputPath[512];
            snprintf(outputPath, sizeof(outputPath), "%s/%s", argv[2], filename);
            FILE* outFile = fopen(outputPath, "w");
            if (!outFile) {
                perror("fopen");
                free(decodedContent);
                free(binaryStr);
                free(filename);
                free(bytes);
                _exit(1);
            }

            fprintf(outFile, "%s", decodedContent);
            fclose(outFile);

            free(decodedContent);
            free(binaryStr);
            free(filename);
            free(bytes);
            _exit(0);
        } else {
            int status = 0;
            if (waitpid(pid, &status, 0) == -1) {
                perror("waitpid");
                free(filename);
                free(bytes);
                break;
            }
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                printf("El proceso hijo para %s terminó con código %d\n", filename, WEXITSTATUS(status));
                free(filename);
                free(bytes);
                break;
            }

            printf("Archivo descomprimido: %s (PID %d)\n", filename, pid);
            free(filename);
            free(bytes);
        }
    }

    fclose(inFile);
    free(codes);

    printf("\nDescompresión completada en: %s\n", argv[2]);
    gettimeofday(&endTime, NULL);
    long long totalMs = elapsedMillis(startTime, endTime);
    printf("Tiempo total de descompresión: %lld ms\n", totalMs);
    
    return 0;
}