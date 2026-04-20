#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "huffman.h"

/* ─── Bloque de datos de un archivo comprimido ──────────────────────────── */
typedef struct {
    char         rel_path[PATH_BUF_SIZE];
    uint64_t     original_size;
    uint64_t     freq[256];
    uint8_t     *data;
    size_t       data_len;
    HuffmanNode *tree;       /* arbol pre-construido por el hilo principal */
    int          success;
    const char      *output_dir;
    pthread_mutex_t *print_mutex; /* mutex compartido para printf */
} FileBlock;

/*
 * Calcula cuantos bytes ocupa la parte comprimida dado freq[256].
 */
static size_t compute_compressed_bytes(const uint64_t freq[256]) {
    HuffmanNode *root = build_huffman_tree(freq);
    if (!root) return 0;
    HuffCode codes[256];
    memset(codes, 0, sizeof(codes));
    build_codes(root, codes, 0, 0);
    free_huffman_tree(root);
    uint64_t total_bits = 0;
    for (int i = 0; i < 256; i++)
        total_bits += freq[i] * (uint64_t)codes[i].length;
    return (size_t)((total_bits + 7) / 8);
}

/* ─── Funcion de hilo: decodifica un FileBlock y escribe su archivo ──────── */
static void *thread_decode(void *arg) {
    FileBlock *block = (FileBlock *)arg;
    block->success = 0;

    char out_path[PATH_BUF_SIZE];
    snprintf(out_path, sizeof(out_path), "%s/%s", block->output_dir, block->rel_path);

    if (!ensure_parent_dirs(out_path)) {
        fprintf(stderr, "No se pudieron crear directorios para %s\n", out_path);
        return NULL;
    }

    FILE *out = fopen(out_path, "wb");
    if (!out) { perror(out_path); return NULL; }

    if (block->original_size == 0) {
        fclose(out);
        pthread_mutex_lock(block->print_mutex);
        printf("Recuperado: %s\n", block->rel_path);
        pthread_mutex_unlock(block->print_mutex);
        block->success = 1;
        return NULL;
    }

    /* El arbol ya fue construido por el hilo principal — sin malloc concurrente */
    HuffmanNode *root = block->tree;
    if (!root) { fclose(out); return NULL; }

    /*
     * Buffer de decodificacion en memoria: evita millones de fputc() con sus
     * llamadas internas de locking de stdio.  Un solo fwrite() al terminar
     * minimiza la contención de I/O entre hilos.
     */
    uint8_t *decode_buf = malloc(block->original_size);
    if (!decode_buf) { perror("malloc decode_buf"); free_huffman_tree(root); fclose(out); return NULL; }

    /* Lector de bits directo sobre el buffer comprimido (sin fmemopen ni fgetc) */
    const uint8_t *src     = block->data;
    size_t         src_pos = 0;
    uint8_t        bit_buf = 0;
    int            bit_cnt = 0;

#define READ_BIT(dest_bit) do {                         \
    if (bit_cnt == 0) {                                 \
        if (src_pos >= block->data_len) { (dest_bit) = -1; break; } \
        bit_buf = src[src_pos++];                       \
        bit_cnt = 8;                                    \
    }                                                   \
    (dest_bit) = (bit_buf & 0x80) ? 1 : 0;             \
    bit_buf <<= 1;                                      \
    bit_cnt--;                                          \
} while(0)

    /* Caso especial: arbol con un solo simbolo */
    if (root->left && !root->right && root->left->is_leaf) {
        memset(decode_buf, root->left->byte, block->original_size);
    } else {
        /* Decodificacion normal — escribe a decode_buf, no a FILE* */
        for (uint64_t w = 0; w < block->original_size; w++) {
            HuffmanNode *curr = root;
            while (curr && !curr->is_leaf) {
                int bit = -1;
                READ_BIT(bit);
                if (bit < 0) {
                    free(decode_buf); free_huffman_tree(root); fclose(out); return NULL;
                }
                curr = (bit == 0) ? curr->left : curr->right;
            }
            if (!curr) { free(decode_buf); free_huffman_tree(root); fclose(out); return NULL; }
            decode_buf[w] = curr->byte;
        }
    }
#undef READ_BIT

    /* Un solo fwrite: mucho menos contención de I/O que N fputc */
    fwrite(decode_buf, 1, block->original_size, out);
    free(decode_buf);
    free_huffman_tree(root);
    fclose(out);

    pthread_mutex_lock(block->print_mutex);
    printf("Recuperado: %s\n", block->rel_path);
    pthread_mutex_unlock(block->print_mutex);
    block->success = 1;
    return NULL;
}

/* ─── main ──────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <archivo_entrada.bin> <directorio_salida>\n", argv[0]);
        return 1;
    }
    const char *input_file = argv[1];
    const char *output_dir  = argv[2];

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    MKDIR(output_dir);

    FILE *in = fopen(input_file, "rb");
    if (!in) { perror(input_file); return 1; }

    char magic[5] = {0};
    if (fread(magic, 1, 4, in) != 4 || strncmp(magic, HUFF_MAGIC, 4) != 0) {
        fprintf(stderr, "Archivo invalido o formato no soportado.\n");
        fclose(in); return 1;
    }

    uint32_t file_count;
    if (!read_u32(in, &file_count)) { fclose(in); return 1; }

    /* 1. Leer TODOS los bloques en memoria (lectura secuencial obligatoria) */
    FileBlock *blocks = calloc(file_count, sizeof(FileBlock));
    if (!blocks) { perror("calloc"); fclose(in); return 1; }

    pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

    for (uint32_t i = 0; i < file_count; i++) {
        uint16_t path_len;
        if (!read_u16(in, &path_len) || path_len >= PATH_BUF_SIZE) {
            fprintf(stderr, "Error leyendo bloque %u\n", i);
            fclose(in); free(blocks); return 1;
        }
        if (fread(blocks[i].rel_path, 1, path_len, in) != path_len) {
            fclose(in); free(blocks); return 1;
        }
        blocks[i].rel_path[path_len] = '\0';

        if (!read_u64(in, &blocks[i].original_size)) { fclose(in); free(blocks); return 1; }

        for (int j = 0; j < 256; j++)
            if (!read_u64(in, &blocks[i].freq[j])) { fclose(in); free(blocks); return 1; }

        blocks[i].data_len = compute_compressed_bytes(blocks[i].freq);
        if (blocks[i].data_len > 0) {
            blocks[i].data = malloc(blocks[i].data_len);
            if (!blocks[i].data) { perror("malloc"); fclose(in); free(blocks); return 1; }
            if (fread(blocks[i].data, 1, blocks[i].data_len, in) != blocks[i].data_len) {
                fprintf(stderr, "Error leyendo datos comprimidos del bloque %u\n", i);
                fclose(in); free(blocks); return 1;
            }
        } else {
            blocks[i].data = NULL;
        }
        blocks[i].output_dir  = output_dir;
        blocks[i].print_mutex = &print_mutex;
    }
    fclose(in);

    /* 2. Pre-construir todos los arboles Huffman en el hilo principal (serial).
     *    Elimina la contención de malloc cuando 91 hilos llaman a
     *    build_huffman_tree() simultaneamente sobre el mismo heap. */
    for (uint32_t i = 0; i < file_count; i++)
        blocks[i].tree = build_huffman_tree(blocks[i].freq);

    /* 3. Lanzar un hilo por archivo */
    pthread_t *threads = malloc(file_count * sizeof(pthread_t));
    if (!threads) { perror("malloc"); free(blocks); return 1; }

    for (uint32_t i = 0; i < file_count; i++) {
        if (pthread_create(&threads[i], NULL, thread_decode, &blocks[i]) != 0) {
            perror("pthread_create");
            for (uint32_t j = 0; j < i; j++) pthread_join(threads[j], NULL);
            free(threads); free(blocks); return 1;
        }
    }

    /* 4. Esperar a todos los hilos */
    for (uint32_t i = 0; i < file_count; i++)
        pthread_join(threads[i], NULL);
    free(threads);
    pthread_mutex_destroy(&print_mutex);

    int all_ok = 1;
    for (uint32_t i = 0; i < file_count; i++) {
        if (!blocks[i].success) {
            fprintf(stderr, "Fallo al recuperar: %s\n", blocks[i].rel_path);
            all_ok = 0;
        }
        free(blocks[i].data);
    }
    free(blocks);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    long elapsed_ms = (ts_end.tv_sec - ts_start.tv_sec) * 1000L
                    + (ts_end.tv_nsec - ts_start.tv_nsec) / 1000000L;
    printf("Descompresion completada en: %s\n", output_dir);
    printf("Tiempo de ejecucion: %ld ms\n", elapsed_ms);
    return all_ok ? 0 : 1;
}
