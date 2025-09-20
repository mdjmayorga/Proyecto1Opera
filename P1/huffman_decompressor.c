#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_CHARS 256
#define MAX_TREE_HT 256

// Nodo del árbol de Huffman
struct MinHeapNode {
    char data;
    struct MinHeapNode *left, *right;
};

// Información de códigos
struct CodeInfo {
    char character;
    char code[MAX_TREE_HT];
};

// Crea un nuevo nodo
struct MinHeapNode* newNode(char data)
{
    struct MinHeapNode* node = malloc(sizeof(struct MinHeapNode));
    node->left = node->right = NULL;
    node->data = data;
    return node;
}

// Reconstruye el árbol de Huffman desde la tabla de códigos
struct MinHeapNode* buildTreeFromCodes(struct CodeInfo* codes, int codeCount)
{
    struct MinHeapNode* root = newNode('$');
    
    for (int i = 0; i < codeCount; i++) {
        struct MinHeapNode* current = root;
        char* code = codes[i].code;
        
        for (int j = 0; code[j] != '\0'; j++) {
            if (code[j] == '0') {
                if (current->left == NULL) {
                    current->left = newNode('$');
                }
                current = current->left;
            } else {
                if (current->right == NULL) {
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
char* binaryToString(FILE* inFile, int bitLength, int lastBitCount)
{
    int byteCount = (bitLength + 7) / 8;
    char* binStr = malloc(bitLength + 10);
    unsigned char* bytes = malloc(byteCount + 1);
    
    if (!binStr || !bytes) {
        printf("Error: No se pudo reservar memoria\n");
        return NULL;
    }
    
    size_t bytesRead = fread(bytes, 1, byteCount, inFile);
    if (bytesRead != byteCount) {
        printf("Error: Solo se pudieron leer %zu de %d bytes\n", bytesRead, byteCount);
    }
    
    int bitIndex = 0;
    for (int i = 0; i < (int)bytesRead; i++) {
        unsigned char byte = bytes[i];
        int bitsToRead = 8;
        
       
        if (i == (int)bytesRead - 1 && lastBitCount != 8) {
            bitsToRead = lastBitCount;
        }
        
        for (int j = 7; j >= 8 - bitsToRead && bitIndex < bitLength; j--) {
            binStr[bitIndex++] = ((byte >> j) & 1) ? '1' : '0';
        }
    }
    
    binStr[bitIndex] = '\0';
    free(bytes);
    return binStr;
}

// Decodifica usando el árbol de Huffman
char* decode_file(struct MinHeapNode* root, char* s)
{
    if (!root || !s) return NULL;
    
    int len = strlen(s);
    char* ans = malloc(len * 2 + 1); 
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
            break;
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
    
    FILE* inFile = fopen(argv[1], "rb");
    if (!inFile) {
        printf("Error: No se pudo abrir el archivo %s\n", argv[1]);
        return 1;
    }
    
    // Crear directorio de salida
    mkdir(argv[2], 0755);
    
    // Leer cabecera
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
    
    // Leer tabla de códigos
    struct CodeInfo* codes = malloc(codeCount * sizeof(struct CodeInfo));
    for (int i = 0; i < codeCount; i++) {
        if (fread(&codes[i].character, sizeof(char), 1, inFile) != 1) {
            printf("Error leyendo carácter %d\n", i);
            break;
        }
        
        int codeLen;
        if (fread(&codeLen, sizeof(int), 1, inFile) != 1) {
            printf("Error leyendo longitud de código %d\n", i);
            break;
        }
        
        if (codeLen <= 0 || codeLen >= MAX_TREE_HT) {
            printf("Error: Longitud de código inválida: %d\n", codeLen);
            break;
        }
        
        if (fread(codes[i].code, sizeof(char), codeLen, inFile) != codeLen) {
            printf("Error leyendo código %d\n", i);
            break;
        }
        codes[i].code[codeLen] = '\0';
        
        printf("Código: '%c' -> %s\n", codes[i].character, codes[i].code);
    }
    
    // Reconstruir árbol de Huffman
    struct MinHeapNode* root = buildTreeFromCodes(codes, codeCount);
    
    // Descomprimir cada archivo
    for (int i = 0; i < fileCount; i++) {
        printf("\nProcesando archivo %d/%d...\n", i+1, fileCount);
        
        // Leer nombre del archivo
        int nameLen;
        if (fread(&nameLen, sizeof(int), 1, inFile) != 1) {
            printf("Error leyendo longitud del nombre\n");
            break;
        }
        
        if (nameLen <= 0 || nameLen > 1000) {
            printf("Error: Longitud de nombre inválida: %d\n", nameLen);
            break;
        }
        
        char* filename = malloc(nameLen + 1);
        if (fread(filename, sizeof(char), nameLen, inFile) != nameLen) {
            printf("Error leyendo nombre del archivo\n");
            free(filename);
            break;
        }
        filename[nameLen] = '\0';
        
        // Leer longitud de datos codificados
        int encodedLen;
        if (fread(&encodedLen, sizeof(int), 1, inFile) != 1) {
            printf("Error leyendo longitud codificada\n");
            free(filename);
            break;
        }
        
        printf("Archivo: %s, bits codificados: %d\n", filename, encodedLen);
        
        // Calcular bytes necesarios y leer lastBitCount
        int byteCount = (encodedLen + 7) / 8;
        unsigned char* bytes = malloc(byteCount + 1);
        
        if (fread(bytes, 1, byteCount, inFile) != byteCount) {
            printf("Error leyendo datos binarios\n");
            free(filename);
            free(bytes);
            break;
        }
        
        int lastBitCount;
        if (fread(&lastBitCount, sizeof(int), 1, inFile) != 1) {
            printf("Error leyendo lastBitCount\n");
            free(filename);
            free(bytes);
            break;
        }
        
        // Convertir a string binario
        char* binaryStr = malloc(encodedLen + 10);
        int bitIndex = 0;
        
        for (int b = 0; b < byteCount && bitIndex < encodedLen; b++) {
            unsigned char byte = bytes[b];
            int bitsToRead = (b == byteCount - 1) ? lastBitCount : 8;
            
            for (int bit = 7; bit >= 8 - bitsToRead && bitIndex < encodedLen; bit--) {
                binaryStr[bitIndex++] = ((byte >> bit) & 1) ? '1' : '0';
            }
        }
        binaryStr[bitIndex] = '\0';
        
        // Decodificar
        char* decodedContent = decode_file(root, binaryStr);
        
        if (decodedContent) {
            // Escribir archivo decodificado
            char outputPath[512];
            sprintf(outputPath, "%s/%s", argv[2], filename);
            FILE* outFile = fopen(outputPath, "w");
            if (outFile) {
                fprintf(outFile, "%s", decodedContent);
                fclose(outFile);
                printf("Archivo descomprimido: %s\n", filename);
            }
            free(decodedContent);
        }
        
        free(filename);
        free(bytes);
        free(binaryStr);
    }
    
    fclose(inFile);
    free(codes);
    
    printf("\nDescompresión completada en: %s\n", argv[2]);
    return 0;
}