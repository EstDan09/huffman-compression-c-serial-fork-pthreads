#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "huffman.h"

typedef struct
{
    HuffmanNode **data;
    size_t size;
    size_t capacity;
} MinHeap;

static HuffmanNode *create_node(unsigned char byte, uint64_t freq, int is_leaf, HuffmanNode *left, HuffmanNode *right)
{
    HuffmanNode *node = (HuffmanNode *)malloc(sizeof(HuffmanNode));

    if (!node)
    {
        fprintf(stderr, "Error allocating memory for HuffmanNode\n");
        return NULL;
    }

    node->byte = byte;
    node->freq = freq;
    node->is_leaf = is_leaf;
    node->left = left;
    node->right = right;

    return node;
}

static void min_heap_init(MinHeap *heap)
{
    heap->data = NULL;
    heap->size = 0;
    heap->capacity = 0;
}

static void heap_free(MinHeap *heap)
{
    free(heap->data);
    heap->data = NULL;
    heap->size = 0;
    heap->capacity = 0;
}

static int heap_push(MinHeap *heap, HuffmanNode *node)
{
    if (heap->size == heap->capacity)
    {
        size_t new_capacity = heap->capacity == 0 ? 16 : heap->capacity * 2;
        HuffmanNode **temporal = (HuffmanNode **)realloc(heap->data, new_capacity * sizeof(HuffmanNode *));

        if (!temporal)
        {
            fprintf(stderr, "Error reallocating memory for MinHeap\n");
            return 0;
        }

        heap->data = temporal;
        heap->capacity = new_capacity;
    }

    size_t i = heap->size++;
    heap->data[i] = node;

    while (i > 0)
    {
        size_t parent = (i - 1) / 2;

        if (heap->data[i]->freq >= heap->data[parent]->freq)
        {
            break;
        }

        HuffmanNode *temp = heap->data[i];
        heap->data[i] = heap->data[parent];
        heap->data[parent] = temp;
        i = parent;
    }

    return 1;
}

static HuffmanNode *heap_pop(MinHeap *heap)
{
    if (heap->size == 0)
    {
        return NULL;
    }

    HuffmanNode *min_node = heap->data[0];
    heap->data[0] = heap->data[--heap->size];

    size_t i = 0;

    while (1)
    {
        size_t left = 2 * i + 1;
        size_t right = 2 * i + 2;
        size_t smallest = i;

        if (left < heap->size && heap->data[left]->freq < heap->data[smallest]->freq)
        {
            smallest = left;
        }

        if (right < heap->size && heap->data[right]->freq < heap->data[smallest]->freq)
        {
            smallest = right;
        }

        if (smallest == i)
        {
            break;
        }

        HuffmanNode *temp = heap->data[i];
        heap->data[i] = heap->data[smallest];
        heap->data[smallest] = temp;
        i = smallest;
    }

    return min_node;
}

HuffmanNode *build_huffman_tree(const uint64_t *freq_table)
{
    MinHeap heap;
    min_heap_init(&heap);

    for (int i = 0; i < 256; i++)
    {
        if (freq_table[i] > 0)
        {
            HuffmanNode *node = create_node((unsigned char)i, freq_table[i], 1, NULL, NULL);

            if (!node)
            {
                heap_free(&heap);
                return NULL;
            }

            if (!heap_push(&heap, node))
            {
                free(node);
                heap_free(&heap);
                return NULL;
            }
        }
    }

    while (heap.size > 1)
    {
        HuffmanNode *left = heap_pop(&heap);
        HuffmanNode *right = heap_pop(&heap);

        if (!left || !right)
        {
            heap_free(&heap);
            return NULL;
        }

        HuffmanNode *internal_node = create_node(0, left->freq + right->freq, 0, left, right);

        if (!internal_node)
        {
            free(left);
            free(right);
            heap_free(&heap);
            return NULL;
        }

        if (!heap_push(&heap, internal_node))
        {
            free(internal_node);
            free(left);
            free(right);
            heap_free(&heap);
            return NULL;
        }
    }

    HuffmanNode *root = heap_pop(&heap);
    heap_free(&heap);
    return root;
}

void build_codes(HuffmanNode *root, HuffCode codes[256], uint64_t bits, uint8_t length)
{
    if (root->is_leaf)
    {
        codes[root->byte].bits = bits;
        codes[root->byte].length = length;
        return;
    }

    build_codes(root->left, codes, (bits << 1) | 0, length + 1);
    build_codes(root->right, codes, (bits << 1) | 1, length + 1);
}

void free_huffman_tree(HuffmanNode *root)
{
    if (!root)
    {
        return;
    }

    free_huffman_tree(root->left);
    free_huffman_tree(root->right);
    free(root);
}

void bit_writer_init(BitWriter *bw, FILE *fp)
{
    bw->buffer = 0;
    bw->bit_count = 0;
    bw->fp = fp;
}

void bit_writer_write_bits(BitWriter *bw, uint64_t bits, uint8_t length)
{
    for (int i = length - 1; i >= 0; i--)
    {
        int bit = (bits >> i) & 1;
        bw->buffer = (bw->buffer << 1) | (uint8_t)bit;
        bw->bit_count++;
        if (bw->bit_count == 8)
        {
            fputc(bw->buffer, bw->fp);
            bw->buffer = 0;
            bw->bit_count = 0;
        }
    }
}

void bit_writer_flush(BitWriter *bw)
{
    if (bw->bit_count > 0)
    {
        bw->buffer <<= (8 - bw->bit_count);
        fputc(bw->buffer, bw->fp);
        bw->buffer = 0;
        bw->bit_count = 0;
    }
}

void bit_reader_init(BitReader *br, FILE *fp)
{
    br->buffer = 0;
    br->bit_count = 0;
    br->fp = fp;
}

int bit_reader_read_bit(BitReader *br)
{
    if (br->bit_count == 0)
    {
        int c = fgetc(br->fp);
        if (c == EOF)
            return -1;
        br->buffer = (uint8_t)c;
        br->bit_count = 8;
    }
    int bit = (br->buffer & 0x80) ? 1 : 0;
    br->buffer <<= 1;
    br->bit_count--;
    return bit;
}

int write_u16(FILE *fp, uint16_t value)
{
    return fwrite(&value, sizeof(value), 1, fp) == 1;
}

int write_u32(FILE *fp, uint32_t value)
{
    return fwrite(&value, sizeof(value), 1, fp) == 1;
}

int write_u64(FILE *fp, uint64_t value)
{
    return fwrite(&value, sizeof(value), 1, fp) == 1;
}

int read_u16(FILE *fp, uint16_t *value)
{
    return fread(value, sizeof(*value), 1, fp) == 1;
}

int read_u32(FILE *fp, uint32_t *value)
{
    return fread(value, sizeof(*value), 1, fp) == 1;
}

int read_u64(FILE *fp, uint64_t *value)
{
    return fread(value, sizeof(*value), 1, fp) == 1;
}

int is_text_file(const char *name)
{
    const char *ext = strrchr(name, '.');
    if (!ext)
        return 0;
    return strcmp(ext, ".txt") == 0 ||
           strcmp(ext, ".md") == 0 ||
           strcmp(ext, ".csv") == 0 ||
           strcmp(ext, ".json") == 0 ||
           strcmp(ext, ".xml") == 0 ||
           strcmp(ext, ".html") == 0 ||
           strcmp(ext, ".log") == 0;
}

int ensure_parent_dirs(const char *filepath)
{
    char path[PATH_BUF_SIZE];
    size_t len = strlen(filepath);
    if (len >= sizeof(path))
        return 0;
    strcpy(path, filepath);
    for (size_t i = 1; i < len; i++)
    {
        if (path[i] == '/' || path[i] == '\\')
        {
            char old = path[i];
            path[i] = '\0';
            if (strlen(path) > 0)
            {
                if (MKDIR(path) != 0 && errno != EEXIST)
                {
                    path[i] = old;
                    return 0;
                }
            }
            path[i] = old;
        }
    }
    return 1;
}