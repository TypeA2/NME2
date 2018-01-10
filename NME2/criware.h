#pragma once

#include "defs.h"
#include "bitmanip.h"

#define COLUMN_STORAGE_MASK         0xf0
#define COLUMN_STORAGE_PERROW       0x50
#define COLUMN_STORAGE_CONSTANT     0x30
#define COLUMN_STORAGE_ZERO         0x10

#define COLUMN_TYPE_MASK            0x0f
#define COLUMN_TYPE_DATA            0x0b
#define COLUMN_TYPE_STRING          0x0a
#define COLUMN_TYPE_FLOAT           0x08
#define COLUMN_TYPE_8BYTE           0x06
#define COLUMN_TYPE_4BYTE2          0x05
#define COLUMN_TYPE_4BYTE           0x04
#define COLUMN_TYPE_2BYTE2          0x03
#define COLUMN_TYPE_2BYTE           0x02
#define COLUMN_TYPE_1BYTE2          0x01
#define COLUMN_TYPE_1BYTE           0x00

#define DXT1 1
#define DXT5 5

#define GET_NEXT_BITS(n) get_next_bits(infile, &input_offset, &bit_pool, &bits_left, n)
#define PACK_RGBA(r, g, b, a) ((r << 24) | (g << 16) | (b << 8) | a);

typedef struct {
    uint32_t offset;
    uint32_t size;
} offset_size_pair;

typedef struct {
    const char* name;
    int index;
} utf_query;

typedef struct {
    bool valid;
    bool found;
    int type;

    union {
        uint64_t value_u64;
        uint32_t value_u32;
        uint16_t value_u16;
        uint8_t  value_u8;
        float    value_float;
        offset_size_pair value_data;
        uint32_t value_string;
    } value;

    uint32_t rows;
    uint32_t name_offset;
    uint32_t string_table_offset;
    uint32_t data_offset;
} utf_query_result;

typedef struct {
    uint8_t type;
    const char* column_name;
    long constant_offset;
} utf_column_info;

typedef struct {
    long table_offset;
    uint32_t table_size;
    uint32_t schema_offset;
    uint32_t rows_offset;
    uint32_t string_table_offset;
    uint32_t data_offset;
    const char* string_table;
    const char* table_name;
    uint16_t columns;
    uint16_t row_width;
    uint32_t rows;
    
    const utf_column_info* schema;
} utf_table_info;

typedef struct {
    char* buffer;
    uint64_t size;
} uncompress_result;

typedef struct {
    uint32_t size;
    uint32_t flags;
    uint32_t fourcc;
    uint32_t rgb_bit_count;
    uint32_t r_bitmask;
    uint32_t g_bitmask;
    uint32_t b_bitmask;
    uint32_t a_bitmask;
} DDS_PIXELFORMAT;

typedef struct {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitch_or_linear_size;
    uint32_t depth;
    uint32_t mip_map_count;
    uint32_t reserverd[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserverd2;
} DDS_HEADER;

typedef int8_t dxt_t;

// Unpack a .cpk file
bool unpack_cpk(FILE* infile, File* file, uint64_t file_size);

utf_query_result query_utf_nofail(FILE* infile, uint64_t offset, utf_query* query);

utf_query_result query_utf_key(FILE* infile, const uint64_t offset, int index, const char* name);

uint64_t query_utf_8byte(FILE* infile, const uint64_t offset, int index, const char* name);

uint32_t query_utf_4byte(FILE* infile, const uint64_t offset, int index, const char* name);

char* load_utf_string_table(FILE* file, const uint64_t offset);

const char* query_utf_string(FILE* infile, const uint64_t offset, int index, const char* name, const char* string_table);

void dump(FILE* infile, FILE* outfile, uint64_t offset, size_t size);

void dump_membuf(FILE* infile, membuf* outbuf, uint64_t offset, size_t size);

static inline uint64_t get_next_bits(FILE* infile, long* const offset_p, uint8_t* const bit_pool_p, int* const bits_left_p, const int bit_count) {
    uint16_t out_bits = 0;
    int num_bits_produced = 0;
    while (num_bits_produced < bit_count) {
        if (*bits_left_p == 0) {
            fseek(infile, *offset_p, SEEK_SET);
            *bit_pool_p = read_8_fs(infile);
            *bits_left_p = 8;
            --*offset_p;
        }

        int bits_this_round;
        if (*bits_left_p > (bit_count - num_bits_produced)) {
            bits_this_round = bit_count - num_bits_produced;
        } else {
            bits_this_round = *bits_left_p;
        }

        out_bits <<= bits_this_round;
        out_bits |= (*bit_pool_p >> (*bits_left_p - bits_this_round)) & ((1 << bits_this_round) - 1);

        *bits_left_p -= bits_this_round;
        num_bits_produced += bits_this_round;
    }

    return out_bits;
}

uncompress_result uncompress(FILE *infile, uint64_t offset, uint64_t input_size);

DDS_HEADER read_dds_header_membuf(membuf* buf);

void decompress_dxt_block(uint32_t x, uint32_t y, uint32_t width, char* block, uint32_t* output, dxt_t type);

void decompress_dxt(uint32_t width, uint32_t height, char* blocks, uint32_t* output, dxt_t type);
