# huffman-compression-c-serial-fork-pthreads

## Overview

This project implements the Huffman Coding algorithm in C for lossless compression of text files. It includes a complete system capable of compressing an entire directory into a single binary file and reconstructing the original structure through decompression.

The implementation supports UTF-8 text, including punctuation marks, accented characters, and special symbols, by operating at the byte level.

Additionally, the project explores different execution models:
- Serial execution
- Parallel execution using `fork()`
- Concurrent execution using `pthread()`

All versions produce identical results and measure execution time in milliseconds.

---

## Features

- Compress all text files from a directory into a single binary file
- Reconstruct original directory structure during decompression
- Supports UTF-8 encoded text (accents, symbols, punctuation)
- Huffman tree construction based on frequency analysis
- Three execution models:
  - Serial
  - Parallel (`fork()`)
  - Concurrent (`pthread()`)
- Execution time measurement in milliseconds

---

## How Huffman Works (Brief)

Huffman coding assigns variable-length binary codes to symbols based on their frequency:

- More frequent characters → shorter codes  
- Less frequent characters → longer codes  

Steps:
1. Count frequency of each byte
2. Build a priority queue (min-heap)
3. Construct a binary tree
4. Generate codes from tree paths
5. Encode data into a bit stream

Decompression rebuilds the tree and decodes the bit stream back to the original content.

---

## Project Structure

```bash
huffman/
├── compress_huff.c
├── decompress_huff.c
├── huffman_common.c
├── huffman_common.h
├── Makefile
└── README.md