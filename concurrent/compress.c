#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#include "huffman.h"

/* ─── FileList ──────────────────────────────────────────────────────────── */
typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} FileList;

static void file_list_init(FileList *list) {
    list->items = NULL; list->count = 0; list->capacity = 0;
}
static void file_list_free(FileList *list) {
    for (size_t i = 0; i < list->count; i++) free(list->items[i]);
    free(list->items);
}
static int file_list_add(FileList *list, const char *path) {
    if (list->count == list->capacity) {
        size_t nc = list->capacity == 0 ? 16 : list->capacity * 2;
        char **tmp = realloc(list->items, nc * sizeof(char *));
        if (!tmp) return 0;
        list->items = tmp; list->capacity = nc;
    }
    list->items[list->count] = strdup(path);
    if (!list->items[list->count]) return 0;
    list->count++;
    return 1;
}

/* ─── Recoleccion de archivos ───────────────────────────────────────────── */
static int collect_files_recursive(const char *base_dir, const char *rel_dir, FileList *list) {
    char full_path[PATH_BUF_SIZE];
    if (rel_dir[0] == '\0')
        snprintf(full_path, sizeof(full_path), "%s", base_dir);
    else
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, rel_dir);

    DIR *dir = opendir(full_path);
    if (!dir) { perror("opendir"); return 0; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char rel_path[PATH_BUF_SIZE], abs_path[PATH_BUF_SIZE];
        if (rel_dir[0] == '\0')
            snprintf(rel_path, sizeof(rel_path), "%s", entry->d_name);
        else
            snprintf(rel_path, sizeof(rel_path), "%s/%s", rel_dir, entry->d_name);
        snprintf(abs_path, sizeof(abs_path), "%s/%s", base_dir, rel_path);
        struct stat st;
        if (stat(abs_path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!collect_files_recursive(base_dir, rel_path, list)) { closedir(dir); return 0; }
        } else if (S_ISREG(st.st_mode)) {
            if (is_text_file(entry->d_name))
                if (!file_list_add(list, rel_path)) { closedir(dir); return 0; }
        }
    }
    closedir(dir);
    return 1;
}

/* ─── Compresion de un archivo (identico al serial) ────────────────────── */
static int compute_frequencies(const char *filepath, uint64_t freq[256], uint64_t *original_size) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) { perror(filepath); return 0; }
    memset(freq, 0, 256 * sizeof(uint64_t));
    *original_size = 0;
    unsigned char buffer[IO_BUF_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        for (size_t i = 0; i < n; i++) freq[buffer[i]]++;
        *original_size += n;
    }
    fclose(fp);
    return 1;
}

static int compress_one_file(FILE *out, const char *base_dir, const char *rel_path) {
    char full_path[PATH_BUF_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, rel_path);

    uint64_t freq[256], original_size;
    if (!compute_frequencies(full_path, freq, &original_size)) return 0;

    HuffmanNode *root = build_huffman_tree(freq);
    HuffCode codes[256];
    memset(codes, 0, sizeof(codes));
    if (root) build_codes(root, codes, 0, 0);

    uint16_t path_len = (uint16_t)strlen(rel_path);
    if (!write_u16(out, path_len)) return 0;
    if (fwrite(rel_path, 1, path_len, out) != path_len) return 0;
    if (!write_u64(out, original_size)) return 0;
    for (int i = 0; i < 256; i++)
        if (!write_u64(out, freq[i])) return 0;

    FILE *in = fopen(full_path, "rb");
    if (!in) { perror(full_path); free_huffman_tree(root); return 0; }
    if (original_size > 0 && !root) { fclose(in); return 0; }

    BitWriter bw;
    bit_writer_init(&bw, out);
    unsigned char buffer[IO_BUF_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)
        for (size_t i = 0; i < n; i++)
            bit_writer_write_bits(&bw, codes[buffer[i]].bits, codes[buffer[i]].length);
    bit_writer_flush(&bw);
    fclose(in);
    free_huffman_tree(root);
    return 1;
}

/* ─── Argumento para cada hilo ──────────────────────────────────────────── */
typedef struct {
    const char *base_dir;
    const char *rel_path;
    char       *out_buf;    /* buffer de salida (open_memstream lo gestiona) */
    size_t      out_size;   /* tamaño del buffer tras comprimir */
    int         success;
    pthread_mutex_t *print_mutex; /* mutex compartido para printf */
} CompressArg;

/* ─── Funcion de hilo ───────────────────────────────────────────────────── */
static void *thread_compress(void *arg) {
    CompressArg *a = (CompressArg *)arg;
    a->out_buf  = NULL;
    a->out_size = 0;

    /*
     * open_memstream crea un FILE* que escribe en un buffer dinamico.
     * El buffer se finaliza al hacer fclose (agrega '\0' y actualiza size).
     * REGION CRITICA: cada hilo tiene su propio FILE*, no hay acceso compartido aqui.
     */
    FILE *mem_fp = open_memstream(&a->out_buf, &a->out_size);
    if (!mem_fp) { a->success = 0; return NULL; }

    a->success = compress_one_file(mem_fp, a->base_dir, a->rel_path);
    fclose(mem_fp); /* finaliza el buffer */

    /* stdout es region critica: proteger con mutex */
    pthread_mutex_lock(a->print_mutex);
    if (a->success)
        printf("Comprimido (hilo): %s\n", a->rel_path);
    else
        fprintf(stderr, "Error comprimiendo: %s\n", a->rel_path);
    pthread_mutex_unlock(a->print_mutex);

    return NULL;
}

/* ─── main ──────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <directorio_entrada> <archivo_salida.bin>\n", argv[0]);
        return 1;
    }
    const char *input_dir   = argv[1];
    const char *output_file = argv[2];

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    /* 1. Recolectar archivos */
    FileList list;
    file_list_init(&list);
    if (!collect_files_recursive(input_dir, "", &list)) return 1;

    if (list.count == 0) {
        fprintf(stderr, "No se encontraron archivos de texto en %s\n", input_dir);
        file_list_free(&list);
        return 1;
    }

    /* 2. Preparar argumentos e hilos */
    pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
    CompressArg *args    = calloc(list.count, sizeof(CompressArg));
    pthread_t   *threads = malloc(list.count * sizeof(pthread_t));
    if (!args || !threads) { perror("malloc"); file_list_free(&list); return 1; }

    for (size_t i = 0; i < list.count; i++) {
        args[i].base_dir    = input_dir;
        args[i].rel_path    = list.items[i];
        args[i].print_mutex = &print_mutex;
    }

    /* 3. Lanzar hilos — cada uno comprime su archivo de forma independiente */
    for (size_t i = 0; i < list.count; i++) {
        if (pthread_create(&threads[i], NULL, thread_compress, &args[i]) != 0) {
            perror("pthread_create");
            /* esperar hilos ya lanzados */
            for (size_t j = 0; j < i; j++) pthread_join(threads[j], NULL);
            free(args); free(threads); file_list_free(&list); return 1;
        }
    }

    /* 4. Esperar a todos los hilos */
    for (size_t i = 0; i < list.count; i++)
        pthread_join(threads[i], NULL);
    free(threads);
    pthread_mutex_destroy(&print_mutex);

    /* Verificar que todos tuvieron exito */
    int all_ok = 1;
    for (size_t i = 0; i < list.count; i++)
        if (!args[i].success) all_ok = 0;

    if (!all_ok) {
        for (size_t i = 0; i < list.count; i++) free(args[i].out_buf);
        free(args); file_list_free(&list); return 1;
    }

    /* 5. Hilo principal ensambla el archivo de salida (secuencial, sin contention) */
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        perror(output_file);
        for (size_t i = 0; i < list.count; i++) free(args[i].out_buf);
        free(args); file_list_free(&list); return 1;
    }

    fwrite(HUFF_MAGIC, 1, 4, out);
    if (!write_u32(out, (uint32_t)list.count)) {
        fclose(out); free(args); file_list_free(&list); return 1;
    }

    for (size_t i = 0; i < list.count; i++) {
        fwrite(args[i].out_buf, 1, args[i].out_size, out);
        free(args[i].out_buf);
    }
    fclose(out);
    free(args);
    file_list_free(&list);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    long elapsed_ms = (ts_end.tv_sec - ts_start.tv_sec) * 1000L
                    + (ts_end.tv_nsec - ts_start.tv_nsec) / 1000000L;
    printf("Archivo generado: %s\n", output_file);
    printf("Tiempo de ejecucion: %ld ms\n", elapsed_ms);
    return 0;
}
