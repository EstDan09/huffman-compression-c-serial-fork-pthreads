#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#include "huffman.h"

typedef struct
{
    char **items;
    size_t count;
    size_t capacity;
} FileList;

static void file_list_init(FileList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}
static void file_list_free(FileList *list)
{
    for (size_t i = 0; i < list->count; i++)
        free(list->items[i]);
    free(list->items);
}
static int file_list_add(FileList *list, const char *path)
{
    if (list->count == list->capacity)
    {
        size_t new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
        char **tmp = (char **)realloc(list->items, new_cap * sizeof(char *));
        if (!tmp)
            return 0;
        list->items = tmp;
        list->capacity = new_cap;
    }
    list->items[list->count] = strdup(path);
    if (!list->items[list->count])
        return 0;
    list->count++;
    return 1;
}

static int collect_files_recursive(const char *base_dir, const char *rel_dir, FileList *list)
{
    char full_path[PATH_BUF_SIZE];
    if (rel_dir[0] == '\0')
    {
        snprintf(full_path, sizeof(full_path), "%s", base_dir);
    }
    else
    {
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, rel_dir);
    }
    DIR *dir = opendir(full_path);
    if (!dir)
    {
        perror("opendir");
        return 0;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char rel_path[PATH_BUF_SIZE];
        char abs_path[PATH_BUF_SIZE];
        if (rel_dir[0] == '\0')
        {
            snprintf(rel_path, sizeof(rel_path), "%s", entry->d_name);
        }
        else
        {
            snprintf(rel_path, sizeof(rel_path), "%s/%s", rel_dir, entry->d_name);
        }
        snprintf(abs_path, sizeof(abs_path), "%s/%s", base_dir, rel_path);
        struct stat st;
        if (stat(abs_path, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
        {
            if (!collect_files_recursive(base_dir, rel_path, list))
            {
                closedir(dir);
                return 0;
            }
        }
        else if (S_ISREG(st.st_mode))
        {
            if (is_text_file(entry->d_name))
            {
                if (!file_list_add(list, rel_path))
                {
                    closedir(dir);
                    return 0;
                }
            }
        }
    }
    closedir(dir);
    return 1;
}

static int compute_frequencies(const char *filepath, uint64_t freq[256],
                               uint64_t *original_size)
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp)
    {
        perror(filepath);
        return 0;
    }
    memset(freq, 0, 256 * sizeof(uint64_t));
    *original_size = 0;
    unsigned char buffer[IO_BUF_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        for (size_t i = 0; i < n; i++)
            freq[buffer[i]]++;
        *original_size += n;
    }
    fclose(fp);
    return 1;
}
static int compress_one_file(FILE *out, const char *base_dir, const char *rel_path)
{
    char full_path[PATH_BUF_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, rel_path);
    uint64_t freq[256];
    uint64_t original_size;
    if (!compute_frequencies(full_path, freq, &original_size))
        return 0;
    HuffmanNode *root = build_huffman_tree(freq);
    HuffCode codes[256] = {0};
    if (root)
        build_codes(root, codes, 0, 0);
    uint16_t path_len = (uint16_t)strlen(rel_path);
    if (!write_u16(out, path_len))
        return 0;
    if (fwrite(rel_path, 1, path_len, out) != path_len)
        return 0;
    if (!write_u64(out, original_size))
        return 0;
    for (int i = 0; i < 256; i++)
    {
        if (!write_u64(out, freq[i]))
            return 0;
    }
    FILE *in = fopen(full_path, "rb");
    if (!in)
    {
        perror(full_path);
        free_huffman_tree(root);
        return 0;
    }
    if (original_size > 0 && !root)
    {
        fclose(in);
        return 0;
    }
    BitWriter bw;
    bit_writer_init(&bw, out);
    unsigned char buffer[IO_BUF_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)
    {
        for (size_t i = 0; i < n; i++)
        {
            bit_writer_write_bits(&bw, codes[buffer[i]].bits,
                                  codes[buffer[i]].length);
        }
    }
    bit_writer_flush(&bw);
    fclose(in);
    free_huffman_tree(root);
    return 1;
}
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Uso: %s <directorio_entrada> <archivo_salida.bin>\n",
                argv[0]);
        return 1;
    }
    const char *input_dir = argv[1];
    const char *output_file = argv[2];

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    FileList list;
    file_list_init(&list);
    if (!collect_files_recursive(input_dir, "", &list))
    {
        return 1;
    }
    FILE *out = fopen(output_file, "wb");
    if (!out)
    {
        perror(output_file);
        file_list_free(&list);
        return 1;
    }
    fwrite(HUFF_MAGIC, 1, 4, out);
    if (!write_u32(out, (uint32_t)list.count))
    {
        fclose(out);
        file_list_free(&list);
        return 1;
    }
    for (size_t i = 0; i < list.count; i++)
    {
        if (!compress_one_file(out, input_dir, list.items[i]))
        {
            fprintf(stderr, "Error comprimiendo: %s\n", list.items[i]);
            fclose(out);
            file_list_free(&list);
            return 1;
        }
        // printf("Comprimido: %s\n", list.items[i]);
    }
    fclose(out);
    file_list_free(&list);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    long elapsed_ms = (ts_end.tv_sec - ts_start.tv_sec) * 1000L
                    + (ts_end.tv_nsec - ts_start.tv_nsec) / 1000000L;
    printf("Archivo generado: %s\n", output_file);
    printf("Tiempo de ejecucion: %ld ms\n", elapsed_ms);

    return 0;
}
