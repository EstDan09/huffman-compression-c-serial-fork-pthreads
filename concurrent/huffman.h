#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define HUFF_MAGIC "HUF1"
#define PATH_BUF_SIZE 4096
#define IO_BUF_SIZE 8192

typedef struct HuffmanNode {
    unsigned char byte;
    uint64_t freq;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
    int is_leaf;
} HuffmanNode;

typedef struct {
    uint64_t bits;
    uint8_t length;
} HuffCode;

typedef struct {
    uint8_t buffer;
    int bit_count;
    FILE *fp;
} BitWriter;

typedef struct {
    uint8_t buffer;
    int bit_count;
    FILE *fp;
} BitReader;

//Para el Huffman
HuffmanNode *build_huffman_tree(const uint64_t freq[256]);
void build_codes(HuffmanNode *root, HuffCode codes[256], uint64_t path, uint8_t length);
void free_huffman_tree(HuffmanNode *root);

//Bit writer and reader
void bit_writer_init(BitWriter *bw, FILE *fp);
void bit_writer_write_bits(BitWriter *bw, uint64_t bits, uint8_t length);
void bit_writer_flush(BitWriter *bw);

void bit_reader_init(BitReader *br, FILE *fp);
int bit_reader_read_bit(BitReader *br);

//Binary read/write functions
int write_u16(FILE *fp, uint16_t value);
int write_u32(FILE *fp, uint32_t value);
int write_u64(FILE *fp, uint64_t value);
int read_u16(FILE *fp, uint16_t *value);
int read_u32(FILE *fp, uint32_t *value);
int read_u64(FILE *fp, uint64_t *value);

// Path helpers
int is_text_file(const char *name);
int ensure_parent_dirs(const char *filepath);

#endif
