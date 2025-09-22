#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/time.h>

#define MAX_FILES 100
#define MAX_FILENAME 256
#define MAX_CHARS 256
#define MAX_TREE_HT 256

// -----------------------------------------------------
// *** DEFINICION DE PTHREADS ***

pthread_mutex_t freq_mutex; // Mutex para proteger acceso al mapa de frecuencias

struct ThreadDataCompressor {
    char filepath[MAX_FILENAME];
};

// ------------------------------------------------------

// Estructura para almacenar información de archivos
struct FileInfo {
    char filename[MAX_FILENAME];
    char *content;
    int size;
};

// Estructura para almacenar frecuencias de caracteres
struct FreqMap {
    char character;
    int frequency;
    int used;
};

// Estructura para almacenar códigos de caracteres
struct CodeMap {
    char character;
    char code[MAX_TREE_HT];
    int used;
};

// Nodo del árbol de Huffman
struct MinHeapNode {
    char data;
    int freq;
    struct MinHeapNode *left, *right;
};

// Estructura del heap mínimo
struct MinHeap {
    int size;
    int capacity;
    struct MinHeapNode **array;
};

// Variables globales
struct FreqMap freq[MAX_CHARS];
struct CodeMap codes[MAX_CHARS];
int freqCount = 0;
int codeCount = 0;

char *readFile(const char *filename, int *size);
void calcFreq(char *str, int len);

// Función para calcular el tiempo transcurrido en milisegundos
long long elapsedMillis(struct timeval start, struct timeval end) {
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    if (microseconds < 0) {
        seconds -= 1;
        microseconds += 1000000;
    }
    return seconds * 1000LL + microseconds / 1000LL;
}

// ---------------------------------------------------------------------------------------
// Funcion que ejecutara cada hilo para leer un archivo y calcular frecuencias
void *process_file_compress(void *arg) {
    struct ThreadDataCompressor *data = (struct ThreadDataCompressor *)arg;
    int size;
    char *content = readFile(data->filepath, &size);

    if (content) {
        // --- Paso 1: calcular frecuencias locales ---
        int localFreq[MAX_CHARS] = {0};
        for (int i = 0; i < size; i++) {
            unsigned char c = (unsigned char)content[i];
            localFreq[c]++;
        }

        // --- Paso 2: reducir al arreglo global ---
        pthread_mutex_lock(&freq_mutex);
        for (int i = 0; i < MAX_CHARS; i++) {
            if (localFreq[i] > 0) {
                int found = 0;
                for (int j = 0; j < freqCount; j++) {
                    if (freq[j].used && freq[j].character == (char)i) {
                        freq[j].frequency += localFreq[i];
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    freq[freqCount].character = (char)i;
                    freq[freqCount].frequency = localFreq[i];
                    freq[freqCount].used = 1;
                    freqCount++;
                }
            }
        }
        pthread_mutex_unlock(&freq_mutex);

        free(content);
    }
    free(data);
    return NULL;
}
// ----------------------------------------------------------------------------------------

// Funciones del heap y árbol de Huffman
struct MinHeapNode *newNode(char data, int freq) {
    struct MinHeapNode *node = malloc(sizeof(struct MinHeapNode));
    node->left = node->right = NULL;
    node->data = data;
    node->freq = freq;
    return node;
}

struct MinHeap *createMinHeap(int capacity) {
    struct MinHeap *minHeap = malloc(sizeof(struct MinHeap));
    minHeap->size = 0;
    minHeap->capacity = capacity;
    minHeap->array = malloc(minHeap->capacity * sizeof(struct MinHeapNode *));
    return minHeap;
}

void swapMinHeapNode(struct MinHeapNode **a, struct MinHeapNode **b) {
    struct MinHeapNode *t = *a;
    *a = *b;
    *b = t;
}

void minHeapify(struct MinHeap *minHeap, int idx) {
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < minHeap->size && minHeap->array[left]->freq < minHeap->array[smallest]->freq)
        smallest = left;

    if (right < minHeap->size && minHeap->array[right]->freq < minHeap->array[smallest]->freq)
        smallest = right;

    if (smallest != idx) {
        swapMinHeapNode(&minHeap->array[smallest], &minHeap->array[idx]);
        minHeapify(minHeap, smallest);
    }
}

int isSizeOne(struct MinHeap *minHeap) {
    return (minHeap->size == 1);
}

struct MinHeapNode *extractMin(struct MinHeap *minHeap) {
    struct MinHeapNode *temp = minHeap->array[0];
    minHeap->array[0] = minHeap->array[minHeap->size - 1];
    --minHeap->size;
    minHeapify(minHeap, 0);
    return temp;
}

void insertMinHeap(struct MinHeap *minHeap, struct MinHeapNode *minHeapNode) {
    ++minHeap->size;
    int i = minHeap->size - 1;
    while (i && minHeapNode->freq < minHeap->array[(i - 1) / 2]->freq) {
        minHeap->array[i] = minHeap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap->array[i] = minHeapNode;
}

// Almacena los códigos Huffman
void storeCodes(struct MinHeapNode *root, char *buffer, int depth) {
    if (!root) return;

    if (!root->left && !root->right) { // hoja
        buffer[depth] = '\0';
        codes[codeCount].character = root->data;
        memcpy(codes[codeCount].code, buffer, depth + 1);
        codes[codeCount].used = 1;
        codeCount++;
        return;
    }

    buffer[depth] = '0';
    storeCodes(root->left, buffer, depth + 1);

    buffer[depth] = '1';
    storeCodes(root->right, buffer, depth + 1);
}

// Construye el árbol de Huffman
struct MinHeapNode *buildHuffmanTree() {
    struct MinHeapNode *left, *right, *top;
    struct MinHeap *minHeap = createMinHeap(freqCount);

    for (int i = 0; i < freqCount; i++) {
        if (freq[i].used) {
            insertMinHeap(minHeap, newNode(freq[i].character, freq[i].frequency));
        }
    }

    while (!isSizeOne(minHeap)) {
        left = extractMin(minHeap);
        right = extractMin(minHeap);

        top = newNode('$', left->freq + right->freq);
        top->left = left;
        top->right = right;

        insertMinHeap(minHeap, top);
    }

    struct MinHeapNode *root = extractMin(minHeap);
    char buffer[MAX_TREE_HT];
    storeCodes(root, buffer, 0);

    free(minHeap->array);
    free(minHeap);
    return root;
}

// Obtiene el código de un carácter
char *getCode(char c) {
    for (int i = 0; i < codeCount; i++) {
        if (codes[i].used && codes[i].character == c) {
            return codes[i].code;
        }
    }
    return "";
}

// Lee un archivo de texto
char *readFile(const char *filename, int *size) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(*size + 1);
    fread(content, 1, *size, file);
    content[*size] = '\0';

    fclose(file);
    return content;
}

// Lee todos los archivos .txt del directorio
int readDirectory(const char *dirPath, struct FileInfo *files) {
    DIR *dir = opendir(dirPath);
    if (!dir) {
        printf("Error: No se pudo abrir el directorio %s\n", dirPath);
        return 0;
    }

    struct dirent *entry;
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

// Convierte string binario a bytes
void stringToBinary(const char *binStr, FILE *outFile) {
    int len = strlen(binStr);
    unsigned char byte = 0;
    int bitCount = 0;

    for (int i = 0; i < len; i++) {
        byte = (byte << 1) | (binStr[i] - '0');
        bitCount++;

        if (bitCount == 8) {
            fwrite(&byte, 1, 1, outFile);
            byte = 0;
            bitCount = 0;
        }
    }

    // Escribir bits restantes si los hay
    if (bitCount > 0) {
        byte <<= (8 - bitCount);
        fwrite(&byte, 1, 1, outFile);
        int lastBits = bitCount;
        fwrite(&lastBits, sizeof(int), 1, outFile);
    } else {
        int lastBits = 8;
        fwrite(&lastBits, sizeof(int), 1, outFile);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <directorio_entrada> <archivo_salida.bin>\n", argv[0]);
        return 1;
    }

    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);

    struct FileInfo files[MAX_FILES];
    pthread_t threads[MAX_FILES];
    int thread_count = 0;

    // Inicializar
    memset(freq, 0, sizeof(freq));
    memset(codes, 0, sizeof(codes));
    freqCount = 0;
    codeCount = 0;

    pthread_mutex_init(&freq_mutex, NULL);

    // ---------- Lanzar hilos para calcular frecuencias ----------
    DIR *dir = opendir(argv[1]);
    if (!dir) {
        printf("ERROR: No se pudo abrir el directorio %s\n", argv[1]);
        return 1;
    }

    struct dirent *entry;
    char fullPath[512];

    while ((entry = readdir(dir)) != NULL && thread_count < MAX_FILES) {
        if (strstr(entry->d_name, ".txt") != NULL) {
            struct ThreadDataCompressor *data = malloc(sizeof(struct ThreadDataCompressor));
            sprintf(fullPath, "%s/%s", argv[1], entry->d_name);
            strcpy(data->filepath, fullPath);
            pthread_create(&threads[thread_count], NULL, process_file_compress, data);
            thread_count++;
        }
    }
    closedir(dir);

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&freq_mutex);
    //--------------------------------------------------------------

    int fileCount = readDirectory(argv[1], files);
    if (fileCount == 0) {
        printf("No se encontraron archivos .txt en el directorio %s\n", argv[1]);
        return 1;
    }

    printf("Construyendo árbol de Huffman...\n");
    buildHuffmanTree();

    FILE *outFile = fopen(argv[2], "wb");
    if (!outFile) {
        printf("ERROR: No se pudo crear el archivo de salida\n");
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

    for (int i = 0; i < fileCount; i++) {
        int nameLen = strlen(files[i].filename);
        fwrite(&nameLen, sizeof(int), 1, outFile);
        fwrite(files[i].filename, sizeof(char), nameLen, outFile);

        // Codificar contenido (sin strcat → O(n))
        char *encodedContent = malloc(files[i].size * 20);
        int pos = 0;
        for (int j = 0; j < files[i].size; j++) {
            char *code = getCode(files[i].content[j]);
            int len = strlen(code);
            memcpy(encodedContent + pos, code, len);
            pos += len;
        }
        encodedContent[pos] = '\0';

        int encodedLen = pos;
        fwrite(&encodedLen, sizeof(int), 1, outFile);
        stringToBinary(encodedContent, outFile);

        printf("Archivo %s codificado: %d -> %d bits\n",
               files[i].filename, files[i].size * 8, encodedLen);

        free(encodedContent);
        free(files[i].content);
    }

    fclose(outFile);

    gettimeofday(&endTime, NULL);
    long long totalMs = elapsedMillis(startTime, endTime);
    printf("\nCompresión completada: %s\n", argv[2]);
    printf("Tiempo total de compresión: %lld ms\n", totalMs);

    return 0;
}
