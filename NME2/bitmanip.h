#pragma once

#include "defs.h"
#include "utils.h"

#define HEADER_BYTES 27
#define MAX_SEGMENTS 255
#define SEGMENT_SIZE 255

// Variable width unsigned integer
typedef struct uint_var {
    // The value to represent
    uint32_t value;

    // The number of bits in which value gets represented
    uint64_t n_bits;
} uint_var;

typedef struct ogg_output_stream {
    // Final output stream
    FILE* out_stream;

    // Buffer for individual bits and the final page
    uint8_t bit_buffer;
    uint8_t page_buffer[HEADER_BYTES + MAX_SEGMENTS + SEGMENT_SIZE * MAX_SEGMENTS];

    // Number of bits stored (flush on 8)
    uint32_t bits_stored;

    // Number of bytes in the final payload
    uint32_t payload_bytes;

    uint32_t granule;
    uint32_t seqno;

    bool first;
    bool continued;
} ogg_output_stream;

// A Vorbis packet
typedef struct Packet {
    // The packet's offset
    long offset;

    // The packet's size
    uint16_t size;

    // The packet's granule
    uint32_t absolute_granule;
} Packet;

// A file-like buffer stored in memory
typedef struct membuf {
    // The buffer's data
    char* data;

    // The total size in bytes
    uint64_t size;

    // Current position of the read/write pointer
    uint64_t pos;
} membuf;

// A wrapper to read individual bits from a membuf into variable width integers
typedef struct bit_stream {
    // Underlying buffer
    membuf* data;

    // Buffer for reading bits
    uint8_t bit_buffer;

    // The total number of bits left in the stream
    uint32_t bits_left;

    // Total number of bits read from the stream
    uint64_t total_bits_read;
} bit_stream;

// Codebook library
typedef struct codebook_library {
    // Pointers to the codebook data
    char* codebook_data;

    // Pointers to the codebook offsets
    long* codebook_offsets;

    // Total number of codebooks
    long codebook_count;
} codebook_library;

// Splits the char* at the given delimiter, and returns the index
uint64_t split_bytes(char* search, uint64_t search_len, char* delimiter, uint64_t delimiter_len, uint64_t start);

// Reads 16 bits from a buffer
uint16_t read_16_buf(unsigned char b[2]);

// Reads 16 bits from a membuf struct
uint16_t read_16_membuf(membuf* buf);

// Reads 32 bits from a buffer
uint32_t read_32_buf(unsigned char b[4]);

// Reads 32 bits from a membuf struct
uint32_t read_32_membuf(membuf* buf);

// Creates a uint_var with value v and size bit_size
uint_var new_uint_var(uint32_t v, uint64_t bit_size);

// A bit stream to write a variable number of bits to, instead of bytes at a time
ogg_output_stream new_ogg_output_stream(FILE* stream);

// Write bits.value to the output stream in bits.n_bits bits
void ogg_write(ogg_output_stream* os, uint_var bits);

// Writes a single bit to the output stream
void put_bit(ogg_output_stream* os, bool bit);

// Flushes all bits to the payload buffer
void flush_bits(ogg_output_stream* os);

// Flushes all bits to the output stream
void flush_page(ogg_output_stream* os, bool next_continued, bool last);

// Writes 32 bits to the specified buffer
void write_32(unsigned char b[4], uint32_t v);

// Calculates the CRC32 checksum of the data in buffer, with size bytes
uint32_t checksum(unsigned char* data, int bytes);

// Writes the Vorbis packet header to the stream
void ogg_write_vph(ogg_output_stream* os, uint8_t type);

// Creates a Vorbis packet from the data in data
Packet packet(membuf* data, long offset);

// Returns the packet's offset (adjusted for overhead)
long packet_offset(Packet packet);

// Returns the offset of the next packet (based on this one's size)
long packet_next_offset(Packet packet);

// Creates a new bit_stream to read variable width integers
bit_stream new_bit_stream(membuf* data);

// Reads uv.n_bits of bits from the stram into uv.value
void bs_read(bit_stream* bstream, uint_var* uv);

// Gets a single bit from the stream
bool get_bit(bit_stream* bs);

// Parses the codebook from buf, with size size, and writes output into os
void parse_codebook(bit_stream* buf, int size, ogg_output_stream* os);

// Returns the number of bits required to represent v
int ilog(unsigned int v);

// _book_maptype1_quantvals function from the Vorbis Tremor codec
unsigned int _book_maptype1_quantvals(unsigned int entries, unsigned int dimensions);

// Extracts a single character from the stream
int membufgetc(membuf* buf);