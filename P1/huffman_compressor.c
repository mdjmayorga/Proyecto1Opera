#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdint.h>

#define MAX_FILES    100
#define MAX_FILENAME 256
#define MAX_CHARS    256
#define MAX_TREE_HT  256

// --------------------- Estructuras ---------------------

struct FileInfo {
    char filename[MAX_FILENAME];
    char* content;
    int   size;
};

struct FreqMap {
    char character;
    uint64_t frequency;
    int used;
};

struct CodeMap {
    char character;
    char code[MAX_TREE_HT]; // '\0' al final
    int used;
};

struct MinHeapNode {
    char data;
    uint64_t freq;
    struct MinHeapNode *left, *right;
};

struct MinHeap {
    int size;
    int capacity;
    struct MinHeapNode** array;
};

// ---------------- Variables globales ------------------
static struct FreqMap freqTab[MAX_CHARS];
static struct CodeMap codes[MAX_CHARS];
static int freqCount = 0;
static int codeCount = 0;

// ---------------- Utilidades --------------------------
static long long elapsedMillis(struct timeval start, struct timeval end) {
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    if (microseconds < 0) { seconds -= 1; microseconds += 1000000; }
    return seconds * 1000LL + microseconds / 1000LL;
}

// ---------------- MinHeap -----------------------------
static struct MinHeapNode* newNode(char data, uint64_t freq) {
    struct MinHeapNode* node = (struct MinHeapNode*)malloc(sizeof(struct MinHeapNode));
    if (!node) { perror("malloc"); exit(1); }
    node->left = node->right = NULL;
    node->data = data;
    node->freq = freq;
    return node;
}

static struct MinHeap* createMinHeap(int capacity) {
    struct MinHeap* minHeap = (struct MinHeap*)malloc(sizeof(struct MinHeap));
    if (!minHeap) { perror("malloc"); exit(1); }
    minHeap->size = 0;
    minHeap->capacity = capacity > 0 ? capacity : 1; // evita malloc(0)
    minHeap->array = (struct MinHeapNode**)malloc(minHeap->capacity * sizeof(struct MinHeapNode*));
    if (!minHeap->array) { perror("malloc"); exit(1); }
    return minHeap;
}

static void swapMinHeapNode(struct MinHeapNode** a, struct MinHeapNode** b) {
    struct MinHeapNode* t = *a; *a = *b; *b = t;
}

static void minHeapify(struct MinHeap* minHeap, int idx) {
    int smallest = idx;
    int left = 2 * idx + 1, right = 2 * idx + 2;

    if (left < minHeap->size && minHeap->array[left]->freq < minHeap->array[smallest]->freq)
        smallest = left;
    if (right < minHeap->size && minHeap->array[right]->freq < minHeap->array[smallest]->freq)
        smallest = right;

    if (smallest != idx) {
        swapMinHeapNode(&minHeap->array[smallest], &minHeap->array[idx]);
        minHeapify(minHeap, smallest);
    }
}

static int isSizeOne(struct MinHeap* minHeap) { return (minHeap->size == 1); }

static struct MinHeapNode* extractMin(struct MinHeap* minHeap) {
    // Precondición: size > 0
    struct MinHeapNode* temp = minHeap->array[0];
    minHeap->array[0] = minHeap->array[minHeap->size - 1];
    --minHeap->size;
    if (minHeap->size > 0) minHeapify(minHeap, 0);
    return temp;
}

static void insertMinHeap(struct MinHeap* minHeap, struct MinHeapNode* minHeapNode) {
    int i = minHeap->size++;
    // burbujea hacia arriba
    while (i && minHeapNode->freq < minHeap->array[(i - 1) / 2]->freq) {
        minHeap->array[i] = minHeap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap->array[i] = minHeapNode;
}

// ---------------- Huffman -----------------------------
static void storeCodes(struct MinHeapNode* root, char* buffer, int depth) {
    if (!root) return;

    // hoja
    if (!root->left && !root->right) {
        buffer[depth] = '\0';
        codes[codeCount].character = root->data;
        memcpy(codes[codeCount].code, buffer, (size_t)depth + 1);
        codes[codeCount].used = 1;
        codeCount++;
        return;
    }

    buffer[depth] = '0';
    storeCodes(root->left, buffer, depth + 1);

    buffer[depth] = '1';
    storeCodes(root->right, buffer, depth + 1);
}

static struct MinHeapNode* buildHuffmanTree_safe(void) {
    // Recolectar símbolos usados (garantiza que capacity = usados)
    int usedCount = 0;
    for (int i = 0; i < MAX_CHARS; i++)
        if (freqTab[i].used && freqTab[i].frequency > 0) usedCount++;

    if (usedCount == 0) {
        // No hay datos: no construir árbol
        return NULL;
    }

    // Caso especial: un solo símbolo => código "0"
    if (usedCount == 1) {
        struct MinHeapNode* root = NULL;
        for (int i = 0; i < MAX_CHARS; i++) {
            if (freqTab[i].used && freqTab[i].frequency > 0) {
                root = newNode(freqTab[i].character, freqTab[i].frequency);
                break;
            }
        }
        // Asignar código "0"
        codes[0].character = root->data;
        codes[0].code[0] = '0';
        codes[0].code[1] = '\0';
        codes[0].used = 1;
        codeCount = 1;
        return root;
    }

    struct MinHeap* minHeap = createMinHeap(usedCount);

    for (int i = 0; i < MAX_CHARS; i++) {
        if (freqTab[i].used && freqTab[i].frequency > 0) {
            insertMinHeap(minHeap, newNode(freqTab[i].character, freqTab[i].frequency));
        }
    }

    while (!isSizeOne(minHeap)) {
        struct MinHeapNode* left  = extractMin(minHeap);
        struct MinHeapNode* right = extractMin(minHeap);
        struct MinHeapNode* top   = newNode('\0', left->freq + right->freq);
        top->left = left; top->right = right;
        insertMinHeap(minHeap, top);
    }

    struct MinHeapNode* root = extractMin(minHeap);

    char buffer[MAX_TREE_HT];
    codeCount = 0;
    storeCodes(root, buffer, 0);

    free(minHeap->array);
    free(minHeap);
    return root;
}

// ---------------- Frecuencias -------------------------
// Conteo O(n) usando un bucket de 256 y luego volcamos a freqTab
static void count_all_files_into_buckets(struct FileInfo* files, int fileCount, uint64_t buckets[256], long* totalSize) {
    memset(buckets, 0, 256 * sizeof(uint64_t));
    *totalSize = 0;
    for (int i = 0; i < fileCount; i++) {
        const unsigned char* p = (const unsigned char*)files[i].content;
        for (int k = 0; k < files[i].size; k++) buckets[p[k]]++;
        *totalSize += files[i].size;
    }
}

static void buckets_to_freqtab(const uint64_t buckets[256]) {
    memset(freqTab, 0, sizeof(freqTab));
    freqCount = 0;
    for (int i = 0; i < 256; i++) {
        if (buckets[i] > 0) {
            freqTab[freqCount].character = (char)i;
            freqTab[freqCount].frequency = buckets[i];
            freqTab[freqCount].used = 1;
            freqCount++;
        }
    }
}

static char* getCode(char c) {
    for (int i = 0; i < codeCount; i++) {
        if (codes[i].used && codes[i].character == c) return codes[i].code;
    }
    // Si no se encuentra (no debería pasar), retorna código vacío
    return "";
}

// ---------------- Archivos ----------------------------
static char* readFile(const char* filename, int* size) {
    FILE* file = fopen(filename, "rb");   // binario
    if (!file) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    long fsz = ftell(file);
    if (fsz < 0) { fclose(file); return NULL; }
    *size = (int)fsz;
    rewind(file);

    char* content = (char*)malloc((size_t)(*size) + 1);
    if (!content) { fclose(file); return NULL; }

    size_t n = fread(content, 1, (size_t)*size, file);
    if ((int)n != *size) {
        // lectura parcial; aún así aseguramos terminador para funciones que lo usen
        content[n] = '\0';
    } else {
        content[*size] = '\0';
    }
    fclose(file);
    return content;
}

static int readDirectory(const char* dirPath, struct FileInfo* files) {
    DIR* dir = opendir(dirPath);
    if (!dir) {
        printf("Error: No se pudo abrir el directorio %s\n", dirPath);
        return 0;
    }

    struct dirent* entry;
    int fileCount = 0;
    char fullPath[1024];

    while ((entry = readdir(dir)) != NULL && fileCount < MAX_FILES) {
        // solo .txt (sencillo)
        const char* name = entry->d_name;
        const char* ext  = strstr(name, ".txt");
        if (ext && ext[4] == '\0') {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, name);
            strncpy(files[fileCount].filename, name, MAX_FILENAME - 1);
            files[fileCount].filename[MAX_FILENAME - 1] = '\0';

            files[fileCount].content = readFile(fullPath, &files[fileCount].size);
            if (files[fileCount].content != NULL) {
                printf("Archivo leído: %s (%d bytes)\n", name, files[fileCount].size);
                fileCount++;
            }
        }
    }

    closedir(dir);
    return fileCount;
}

// ---------------- Binario -----------------------------
static void stringToBinary(const char* binStr, FILE* outFile) {
    int len = (int)strlen(binStr);
    unsigned char byte = 0;
    int bitCount = 0;

    for (int i = 0; i < len; i++) {
        byte = (byte << 1) | (unsigned char)(binStr[i] - '0');
        bitCount++;
        if (bitCount == 8) {
            fwrite(&byte, 1, 1, outFile);
            byte = 0;
            bitCount = 0;
        }
    }

    int lastBits;
    if (bitCount > 0) {
        byte <<= (8 - bitCount);
        fwrite(&byte, 1, 1, outFile);
        lastBits = bitCount;
    } else {
        lastBits = 8;
    }
    fwrite(&lastBits, sizeof(int), 1, outFile);
}

// ---------------- Main -------------------------------
int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: %s <directorio_entrada> <archivo_salida.bin>\n", argv[0]);
        return 1;
    }

    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);

    struct FileInfo files[MAX_FILES];
    memset(files, 0, sizeof(files));
    memset(freqTab, 0, sizeof(freqTab));
    memset(codes,   0, sizeof(codes));
    freqCount = 0;
    codeCount = 0;

    // 1) Leer archivos
    int fileCount = readDirectory(argv[1], files);
    if (fileCount == 0) {
        printf("No se encontraron archivos .txt en el directorio\n");
        return 1;
    }

    // 2) Contar frecuencias O(n)
    uint64_t buckets[256];
    long totalSize = 0;
    count_all_files_into_buckets(files, fileCount, buckets, &totalSize);
    buckets_to_freqtab(buckets);

    printf("\nCalculando frecuencias de %ld caracteres... símbolos distintos: %d\n",
           totalSize, freqCount);

    // 3) Construir árbol de Huffman seguro (maneja 0/1 símbolos)
    struct MinHeapNode* root = buildHuffmanTree_safe();
    if (freqCount > 0 && !root) {
        fprintf(stderr, "Error construyendo el árbol de Huffman\n");
        return 1;
    }

    // 4) Archivo de salida
    FILE* outFile = fopen(argv[2], "wb");
    if (!outFile) {
        perror("fopen salida");
        return 1;
    }

    // Cabecera simple: #archivos y #códigos
    fwrite(&fileCount, sizeof(int), 1, outFile);
    fwrite(&codeCount, sizeof(int), 1, outFile);

    // Tabla de códigos
    for (int i = 0; i < codeCount; i++) {
        if (codes[i].used) {
            fwrite(&codes[i].character, sizeof(char), 1, outFile);
            int codeLen = (int)strlen(codes[i].code);
            fwrite(&codeLen, sizeof(int), 1, outFile);
            fwrite(codes[i].code, sizeof(char), (size_t)codeLen, outFile);
        }
    }

    // Determinar longitud máxima de código para dimensionar buffers
    int maxCodeLen = 1;
    for (int i = 0; i < codeCount; i++) {
        if (codes[i].used) {
            int L = (int)strlen(codes[i].code);
            if (L > maxCodeLen) maxCodeLen = L;
        }
    }

    // 5) Codificar cada archivo
    for (int i = 0; i < fileCount; i++) {
        // nombre
        int nameLen = (int)strlen(files[i].filename);
        fwrite(&nameLen, sizeof(int), 1, outFile);
        fwrite(files[i].filename, sizeof(char), (size_t)nameLen, outFile);

        // tamaño conservador del buffer de bits (caracteres * long máx)
        size_t encCap = (size_t)files[i].size * (size_t)maxCodeLen + 1;
        char* encodedContent = (char*)malloc(encCap);
        if (!encodedContent) { perror("malloc encoded"); fclose(outFile); return 1; }

        int pos = 0;
        for (int j = 0; j < files[i].size; j++) {
            char* code = getCode(files[i].content[j]);
            int len = (int)strlen(code);
            if (len > 0) {
                // aseguramos no rebasar (muy conservador por encCap)
                if ((size_t)(pos + len + 1) > encCap) {
                    // re-dimensionar si hiciera falta (raro con encCap correcto)
                    size_t newCap = encCap * 2 + len + 1;
                    char* tmp = (char*)realloc(encodedContent, newCap);
                    if (!tmp) { perror("realloc"); free(encodedContent); fclose(outFile); return 1; }
                    encodedContent = tmp;
                    encCap = newCap;
                }
                memcpy(encodedContent + pos, code, (size_t)len);
                pos += len;
            } else {
                // Caso borde: un solo símbolo en todo el dataset con código "0"
                // len==0 no debería ocurrir con nuestro manejo; por seguridad:
                if (codeCount == 1) {
                    if ((size_t)(pos + 2) > encCap) {
                        size_t newCap = encCap * 2 + 2;
                        char* tmp = (char*)realloc(encodedContent, newCap);
                        if (!tmp) { perror("realloc"); free(encodedContent); fclose(outFile); return 1; }
                        encodedContent = tmp;
                        encCap = newCap;
                    }
                    encodedContent[pos++] = '0';
                }
            }
        }
        encodedContent[pos] = '\0';

        int encodedLen = pos; // bits
        fwrite(&encodedLen, sizeof(int), 1, outFile);

        stringToBinary(encodedContent, outFile);

        printf("Archivo %s codificado: %d -> %d bits\n",
               files[i].filename, files[i].size * 8, encodedLen);

        free(encodedContent);
        free(files[i].content);
        files[i].content = NULL;
    }

    fclose(outFile);

    gettimeofday(&endTime, NULL);
    long long totalMs = elapsedMillis(startTime, endTime);
    printf("\nCompresión completada: %s\n", argv[2]);
    printf("Tiempo total de compresión: %lld ms\n", totalMs);

    return 0;
}
