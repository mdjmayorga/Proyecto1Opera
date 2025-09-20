#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>  // Manejo de hilos
#include <sys/time.h> // Para medir el tiempo

#define MAX_CHARS 256
#define MAX_TREE_HT 256

// Estructura para pasar datos a los hilos del descompresor
struct ThreadDataDecompressor
{
    struct MinHeapNode *root;
    char *encoded_data;
    char output_filename[512];
};

// Nodo del árbol de Huffman
struct MinHeapNode
{
    char data;
    struct MinHeapNode *left, *right;
};

// Información de códigos
struct CodeInfo
{
    char character;
    char code[MAX_TREE_HT];
};

// Función para calcular el tiempo transcurrido en milisegundos
long long elapsedMillis(struct timeval start, struct timeval end)
{
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;

    if (microseconds < 0)
    {
        seconds -= 1;
        microseconds += 1000000;
    }

    return seconds * 1000LL + microseconds / 1000LL;
}

// Crea un nuevo nodo
struct MinHeapNode *newNode(char data)
{
    struct MinHeapNode *node = malloc(sizeof(struct MinHeapNode));
    node->left = node->right = NULL;
    node->data = data;
    return node;
}

// Reconstruye el árbol de Huffman desde la tabla de códigos
struct MinHeapNode *buildTreeFromCodes(struct CodeInfo *codes, int codeCount)
{
    struct MinHeapNode *root = newNode('$');

    for (int i = 0; i < codeCount; i++)
    {
        struct MinHeapNode *current = root;
        char *code = codes[i].code;

        for (int j = 0; code[j] != '\0'; j++)
        {
            if (code[j] == '0')
            {
                if (current->left == NULL)
                {
                    current->left = newNode('$');
                }
                current = current->left;
            }
            else
            {
                if (current->right == NULL)
                {
                    current->right = newNode('$');
                }
                current = current->right;
            }
        }
        current->data = codes[i].character;
    }

    return root;
}

// Convierte bytes a string binario
char *binaryToString(FILE *inFile, int bitLength, int lastBitCount)
{
    int byteCount = (bitLength + 7) / 8;
    char *binStr = malloc(bitLength + 10);
    unsigned char *bytes = malloc(byteCount + 1);

    if (!binStr || !bytes)
    {
        printf("Error: No se pudo reservar memoria\n");
        return NULL;
    }

    size_t bytesRead = fread(bytes, 1, byteCount, inFile);
    if (bytesRead != (size_t)byteCount)
    {
        printf("Error: Solo se pudieron leer %zu de %d bytes\n", bytesRead, byteCount);
    }

    int bitIndex = 0;
    for (int i = 0; i < (int)bytesRead; i++)
    {
        unsigned char byte = bytes[i];
        int bitsToRead = 8;

        if (i == (int)bytesRead - 1 && lastBitCount != 8)
        {
            bitsToRead = lastBitCount;
        }

        for (int j = 7; j >= 8 - bitsToRead && bitIndex < bitLength; j--)
        {
            binStr[bitIndex++] = ((byte >> j) & 1) ? '1' : '0';
        }
    }

    binStr[bitIndex] = '\0';
    free(bytes);
    return binStr;
}

// Decodifica usando el árbol de Huffman
char *decode_file(struct MinHeapNode *root, char *s)
{
    if (!root || !s)
        return NULL;

    int len = strlen(s);
    char *ans = malloc(len * 2 + 1);
    struct MinHeapNode *curr = root;
    int ansIndex = 0;

    for (int i = 0; i < len; i++)
    {
        if (s[i] == '0')
        {
            if (curr->left)
                curr = curr->left;
        }
        else if (s[i] == '1')
        {
            if (curr->right)
                curr = curr->right;
        }

        if (curr && curr->left == NULL && curr->right == NULL && curr->data != '$')
        {
            ans[ansIndex++] = curr->data;
            curr = root;
        }

        if (!curr)
        {
            printf("Error: Árbol corrupto en posición %d\n", i);
            break;
        }
    }
    ans[ansIndex] = '\0';
    return ans;
}

// Función que ejecutará cada hilo para descomprimir un archivo
void *process_file_decompress(void *arg)
{
    struct ThreadDataDecompressor *data = (struct ThreadDataDecompressor *)arg;

    // Decodificar
    char *decoded_content = decode_file(data->root, data->encoded_data);

    if (decoded_content)
    {
        // Escribir el archivo decodificado
        FILE *outFile = fopen(data->output_filename, "w");
        if (outFile)
        {
            fprintf(outFile, "%s", decoded_content);
            fclose(outFile);
            printf("Archivo descomprimido: %s\n", data->output_filename);
        }
        free(decoded_content);
    }

    free(data->encoded_data);
    free(data);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Uso: %s <archivo_comprimido.bin> <directorio_salida>\n", argv[0]);
        return 1;
    }

    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);

    FILE *inFile = fopen(argv[1], "rb");
    if (!inFile)
    {
        printf("ERROR: No se pudo abrir el archivo %s\n", argv[1]);
        return 1;
    }

    // Crear directorio de salida
    mkdir(argv[2], 0755);

    // Leer cabecera
    int fileCount, codeCount;
    if (fread(&fileCount, sizeof(int), 1, inFile) != 1 ||
        fread(&codeCount, sizeof(int), 1, inFile) != 1)
    {
        printf("ERROR: No se pudo leer la cabecera\n");
        fclose(inFile);
        return 1;
    }

    printf("Archivos a descomprimir: %d\n", fileCount);
    printf("Códigos en tabla: %d\n", codeCount);

    if (codeCount <= 0 || codeCount > MAX_CHARS)
    {
        printf("ERROR: Número de códigos inválido: %d\n", codeCount);
        fclose(inFile);
        return 1;
    }

    // Leer tabla de códigos
    struct CodeInfo *codes = malloc(codeCount * sizeof(struct CodeInfo));
    for (int i = 0; i < codeCount; i++)
    {
        if (fread(&codes[i].character, sizeof(char), 1, inFile) != 1)
        {
            printf("Error leyendo carácter %d\n", i);
            break;
        }

        int codeLen;
        if (fread(&codeLen, sizeof(int), 1, inFile) != 1)
        {
            printf("Error leyendo longitud de código %d\n", i);
            break;
        }

        if (codeLen <= 0 || codeLen >= MAX_TREE_HT)
        {
            printf("Error: Longitud de código inválida: %d\n", codeLen);
            break;
        }

        if (fread(codes[i].code, sizeof(char), codeLen, inFile) != (size_t)codeLen)
        {
            printf("Error leyendo código %d\n", i);
            break;
        }
        codes[i].code[codeLen] = '\0';

        printf("Código: '%c' -> %s\n", codes[i].character, codes[i].code);
    }

    // Reconstruir árbol de Huffman
    struct MinHeapNode *root = buildTreeFromCodes(codes, codeCount);

    pthread_t threads[fileCount];

    // Descomprimir cada archivo en un hilo separado
    for (int i = 0; i < fileCount; i++)
    {
        printf("\nLanzando hilo para archivo  %d/%d...\n", i + 1, fileCount);

        // Leer nombre del archivo
        int nameLen;
        if (fread(&nameLen, sizeof(int), 1, inFile) != 1)
        {
            printf("Error leyendo longitud del nombre\n");
            break;
        }

        if (nameLen <= 0 || nameLen > 1000)
        {
            printf("Error: Longitud de nombre inválida: %d\n", nameLen);
            break;
        }

        char *filename = malloc(nameLen + 1);
        if (fread(filename, sizeof(char), nameLen, inFile) != (size_t)nameLen)
        {
            printf("Error leyendo nombre del archivo\n");
            free(filename);
            break;
        }
        filename[nameLen] = '\0';

        // Leer longitud de datos codificados
        int encodedLen;
        if (fread(&encodedLen, sizeof(int), 1, inFile) != 1)
        {
            printf("Error leyendo longitud codificada\n");
            free(filename);
            break;
        }

        printf("Archivo: %s, bits codificados: %d\n", filename, encodedLen);

        // Calcular bytes necesarios y leer lastBitCount
        int byteCount = (encodedLen + 7) / 8;
        unsigned char *bytes = malloc(byteCount + 1);

        if (fread(bytes, 1, byteCount, inFile) != (size_t)byteCount)
        {
            printf("Error leyendo datos binarios\n");
            free(filename);
            free(bytes);
            break;
        }

        int lastBitCount;
        if (fread(&lastBitCount, sizeof(int), 1, inFile) != 1)
        {
            printf("Error leyendo lastBitCount\n");
            free(filename);
            free(bytes);
            break;
        }

        // Convertir a string binario
        char *binaryStr = malloc(encodedLen + 10);
        int bitIndex = 0;

        for (int b = 0; b < byteCount && bitIndex < encodedLen; b++)
        {
            unsigned char byte = bytes[b];
            int bitsToRead = (b == byteCount - 1) ? lastBitCount : 8;

            for (int bit = 7; bit >= 8 - bitsToRead && bitIndex < encodedLen; bit--)
            {
                binaryStr[bitIndex++] = ((byte >> bit) & 1) ? '1' : '0';
            }
        }
        binaryStr[bitIndex] = '\0';

        struct ThreadDataDecompressor *data = malloc(sizeof(struct ThreadDataDecompressor));
        data->root = root;
        data->encoded_data = binaryStr; // string binario generado
        sprintf(data->output_filename, "%s/%s", argv[2], filename);

        // Crear hilo para procesar el archivo
        pthread_create(&threads[i], NULL, process_file_decompress, data);

        free(filename);
        free(bytes);
    }

    // Esperar a que todos los hilos terminen
    for (int i = 0; i < fileCount; i++)
    {
        pthread_join(threads[i], NULL);
    }

    fclose(inFile);
    free(codes);

    gettimeofday(&endTime, NULL);
    long long totalMs = elapsedMillis(startTime, endTime);

    printf("\nDescompresión completada en: %s\n", argv[2]);
    printf("Tiempo total de descompresión: %lld ms\n", totalMs);

    return 0;
}