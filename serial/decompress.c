#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "huffman.h"

static int decode_file(FILE *in, const char *output_dir) {
    uint16_t path_len;
    if (!read_u16(in, &path_len)) return 0;

    char rel_path[PATH_BUF_SIZE];
    if (path_len >= sizeof(rel_path)) return 0;
    if (fread(rel_path, 1, path_len, in) != path_len) return 0;
    rel_path[path_len] = '\0';

    uint64_t original_size;
    if (!read_u64(in, &original_size)) return 0;

    uint64_t freq[256];
    for (int i = 0; i < 256; i++) {
        if (!read_u64(in, &freq[i])) return 0;
    }

    char out_path[PATH_BUF_SIZE];
    snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, rel_path);

    if (!ensure_parent_dirs(out_path)) {
        fprintf(stderr, "No se pudieron crear directorios para %s\n", out_path);
        return 0;
    }

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        perror(out_path);
        return 0;
    }

    if (original_size == 0) {
        fclose(out);
        printf("Recuperado: %s\n", rel_path);
        return 1;
    }

    HuffmanNode *root = build_huffman_tree(freq);
    if (!root) {
        fclose(out);
        return 0;
    }

    BitReader br;
    bit_reader_init(&br, in);

    if (root->left && !root->right && root->left->is_leaf) {
        for (uint64_t i = 0; i < original_size; i++) {
            fputc(root->left->byte, out);
        }
        uint64_t pad_bits = (8 - (original_size % 8)) % 8;
        for (uint64_t i = 0; i < pad_bits; i++) {
            if (bit_reader_read_bit(&br) < 0) break;
        }
        free_huffman_tree(root);
        fclose(out);
        printf("Recuperado: %s\n", rel_path);
        return 1;
    }

    for (uint64_t written = 0; written < original_size; written++) {
        HuffmanNode *curr = root;
        while (curr && !curr->is_leaf) {
            int bit = bit_reader_read_bit(&br);
            if (bit < 0) {
                free_huffman_tree(root);
                fclose(out);
                return 0;
            }
            curr = (bit == 0) ? curr->left : curr->right;
        }
        if (!curr) {
            free_huffman_tree(root);
            fclose(out);
            return 0;
        }
        fputc(curr->byte, out);
    }

    free_huffman_tree(root);
    fclose(out);
    // printf("Recuperado: %s\n", rel_path);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <archivo_entrada.bin> <directorio_salida>\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *output_dir = argv[2];

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    if (MKDIR(output_dir) != 0) {
        // si ya existe no pasa nada
    }

    FILE *in = fopen(input_file, "rb");
    if (!in) {
        perror(input_file);
        return 1;
    }

    char magic[5] = {0};
    if (fread(magic, 1, 4, in) != 4 || strncmp(magic, HUFF_MAGIC, 4) != 0) {
        fprintf(stderr, "Archivo invalido o formato no soportado.\n");
        fclose(in);
        return 1;
    }

    uint32_t file_count;
    if (!read_u32(in, &file_count)) {
        fclose(in);
        return 1;
    }

    for (uint32_t i = 0; i < file_count; i++) {
        if (!decode_file(in, output_dir)) {
            fprintf(stderr, "Error descomprimiendo archivo %u\n", i + 1);
            fclose(in);
            return 1;
        }
    }

    fclose(in);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    long elapsed_ms = (ts_end.tv_sec - ts_start.tv_sec) * 1000L
                    + (ts_end.tv_nsec - ts_start.tv_nsec) / 1000000L;
    printf("Descompresion completada en: %s\n", output_dir);
    printf("Tiempo de ejecucion: %ld ms\n", elapsed_ms);
    return 0;
}