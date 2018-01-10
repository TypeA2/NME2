#include "criware.h"
#include "bitmanip.h"
#include <ddstream.h>

bool unpack_cpk(FILE* infile, File* file, uint64_t file_size) {
    const uint64_t cpk_header_offset = 0x0;
    char* toc_string_table = NULL;

    // Check CpkHeader
    {
        utf_query_result result = query_utf_nofail(infile, cpk_header_offset + 0x10, NULL);

        if (result.rows != 1) {
            perrf("Wrong number of rows in CpkHeader\n");
            
            return false;
        }
    }

    uint64_t toc_offset       = query_utf_8byte(infile, cpk_header_offset + 0x10, 0, "TocOffset");
    uint64_t content_offset   = query_utf_8byte(infile, cpk_header_offset + 0x10, 0, "ContentOffset");
    uint64_t cpk_header_count = query_utf_4byte(infile, cpk_header_offset + 0x10, 0, "Files");

    {
        unsigned char buf[4];
        static const char TOC_signature[4] = "TOC ";
        fseek(infile, (long)toc_offset, SEEK_SET);
        fread_s(buf, 4, 1, 4, infile);

        if (memcmp(buf, TOC_signature, 4) != 0) {
            perrf("TOC signature invalid or not found\n");

            return false;
        }
    }

    long toc_entries;

    {
        utf_query_result result = query_utf_nofail(infile, toc_offset + 0x10, NULL);

        toc_entries = result.rows;
        toc_string_table = load_utf_string_table(infile, toc_offset + 0x10);
    }

    if (toc_entries != cpk_header_count) {
        perrf("CpkHeader file count and TOC entry count do not equal\n");

        return false;
    }
    
    fpath dir;
    strcpy_s(dir.drive, _MAX_DRIVE, file->input.drive);
    sprintf_s(dir.dir, _MAX_DIR, "%s%s\\", file->input.dir, file->input.fname);
    memset(dir.fname, '\0', _MAX_FNAME);
    memset(dir.ext, '\0', _MAX_EXT);
    if (_mkdir(MakePath(dir)) != 0) {
        if (errno != EEXIST) {
            perrf("Could not create directory '%s', error %i\n", MakePath(dir), errno);

            return false;
        }
    }

    for (int i = 0; i < toc_entries; i++) {
        char clear[80] = "";
        memset(clear, ' ', 79);
        printf("\r%s\rUnpacking files: %i of %i", clear, i + 1, toc_entries);
        const char* file_name = query_utf_string(infile, toc_offset + 0x10, i, "FileName", toc_string_table);
        const char* dir_name = query_utf_string(infile, toc_offset + 0x10, i, "DirName", toc_string_table);
        uint32_t file_size = query_utf_4byte(infile, toc_offset + 0x10, i, "FileSize");
        uint32_t extract_size = query_utf_4byte(infile, toc_offset + 0x10, i, "ExtractSize");
        uint64_t file_offset = query_utf_8byte(infile, toc_offset + 0x10, i, "FileOffset");

        printf(" (%s/%s", dir_name, file_name);

        if (content_offset < toc_offset) {
            file_offset += content_offset;
        } else {
            file_offset += toc_offset;
        }
        fpath output;
        memset(output.fname, '\0', _MAX_FNAME);
        memset(output.ext, '\0', _MAX_EXT);
        strcpy_s(output.drive, _MAX_DRIVE, file->input.drive);
        sprintf_s(output.dir, _MAX_DIR, "%s%s\\%s", file->input.dir, file->input.fname, dir_name);
        if (_mkdir(MakePath(output)) != 0) {
            if (errno != EEXIST) {
                perrf(")\nCould not create directory '%s', error %i\n", MakePath(output), errno);

                return false;
            }
        }

        _splitpath_s(file_name, NULL, 0, NULL, 0, output.fname, _MAX_FNAME, output.ext, _MAX_EXT);

        if (_stricmp(output.ext, ".wtp") == 0) {
            membuf outbuf = new_membuf();

            if (extract_size > file_size) {
                printf(", compressed)");
                uncompress_result res = uncompress(infile, file_offset, file_size);

                outbuf.data = res.buffer;
                outbuf.size = res.size;
            } else {
                printf(", uncompressed)");
                dump_membuf(infile, &outbuf, file_offset, file_size);
            }

            char dds_fcc[4] = "DDS ";
            if (memcmp(outbuf.data, dds_fcc, 4) != 0){
                perrf(")\nInvalid DDS file\n");

                return false;
            }
            outbuf.pos += 4;

            DDS_HEADER header = read_dds_header_membuf(&outbuf);

            if (header.size != 124) {
                perrf(")\nHeader size must be 124\n");

                return false;
            }

            if (header.ddspf.size != 32) {
                perrf(")\nPixel format size must be 32\n");

                return false;
            }

            if (header.ddspf.fourcc != 0x35545844 && header.ddspf.fourcc != 0x31545844) {
                char* fcc_str = (char*)&header.ddspf.fourcc;
                perrf("\nInvalid format: %c%c%c%c\n", fcc_str[0], fcc_str[1], fcc_str[2], fcc_str[3]);

                return false;
            }

            bool dxt5 = (header.ddspf.fourcc == 0x35545844);

            uint32_t* output_buffer = malloc(header.width * header.height * sizeof(uint32_t));

            if (dxt5) {
                decompress_dxt(header.width, header.height, &outbuf.data[0x80], output_buffer, DXT5);
            } else {
                decompress_dxt(header.width, header.height, &outbuf.data[0x80], output_buffer, DXT1);
            }

            strcpy_s(output.ext, _MAX_EXT, ".bmp");
            FILE* bmp;
            fopen_s(&bmp, MakePath(output), "wb");

            BITMAPFILEHEADER fh;
            memcpy_s(&fh.bfType, 2, "BM", 2);
            fh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (header.width * header.height * 4);
            fh.bfReserved1 = 0;
            fh.bfReserved2 = 0;
            fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

            BITMAPINFOHEADER ih;
            ih.biSize = sizeof(BITMAPINFOHEADER);
            ih.biWidth = header.width;
            ih.biHeight = header.height;
            ih.biPlanes = 1;
            ih.biBitCount = 32;
            ih.biCompression = 0;
            ih.biSizeImage = 0;
            ih.biXPelsPerMeter = 0;
            ih.biYPelsPerMeter = 0;
            ih.biClrUsed = 0;
            ih.biClrImportant = 0;

            fwrite(&fh, 1, sizeof(BITMAPFILEHEADER), bmp);
            fwrite(&ih, 1, sizeof(BITMAPINFOHEADER), bmp);
            for (int32_t y = header.height - 1; y > -1; y--) {
                for (uint32_t x = 0; x < header.width; x++) {
                    unsigned char pair[4];
                    memcpy_s(pair, 4, &output_buffer[x + y * header.width], 4);

                    unsigned char flipped[4] = {
                        pair[1], pair[2], pair[3], pair[0]
                    };

                    size_t written = fwrite(flipped, 1, 4, bmp);

                    if (written != 4) {
                        perrf("\nWrote %i bytes instead of 4\n", written);

                        return false;
                    }
                }
            }
            fclose(bmp);
            
        } else {
            FILE* outfile;
            fopen_s(&outfile, MakePath(output), "w+b");
            if (extract_size > file_size) {

                printf(", compressed)");

                uncompress_result res = uncompress(infile, file_offset, file_size);

                fseek(outfile, 0, SEEK_SET);

                size_t bytes_written = fwrite(res.buffer, 1, res.size, outfile);

                if (bytes_written != res.size) {
                    perrf("\nCould not write bytes\n");

                    return false;
                }

                free(res.buffer);
            } else {
                printf(", uncompressed)");
                dump(infile, outfile, file_offset, file_size);
            }

            fclose(outfile);
        }
    }

    return true;
}

utf_query_result query_utf_nofail(FILE* infile, uint64_t offset, utf_query* query) {
    utf_table_info table_info;
    utf_query_result result;

    result.valid = false;

    table_info.table_offset = (long)offset;

    const char UTF_signature[4] = "@UTF";

    unsigned char buf[4];
    fseek(infile, (long)offset, SEEK_SET);
    fread_s(buf, 4, 1, 4, infile);
    if (memcmp(buf, UTF_signature, 4) != 0) {
        perrf("Invalid UTF signature\n");

        exit(1);
    }

    table_info.table_size = read_32_fs_be(infile);
    table_info.schema_offset = 0x20;
    table_info.rows_offset = read_32_fs_be(infile);
    table_info.string_table_offset = read_32_fs_be(infile);
    table_info.data_offset = read_32_fs_be(infile);

    const uint32_t table_name_string = read_32_fs_be(infile);

    table_info.columns = read_16_fs_be(infile);
    table_info.row_width = read_16_fs_be(infile);
    table_info.rows = read_32_fs_be(infile);

    const int string_table_size = table_info.data_offset - table_info.string_table_offset;

    char* string_table = malloc(string_table_size + 1);
    table_info.string_table = string_table;
    memset(string_table, '\0', string_table_size + 1);

    utf_column_info* schema = malloc(table_info.columns * sizeof(utf_column_info));

    for (int i = 0; i < table_info.columns; i++) {
        schema[i].type = read_8_fs(infile);
        schema[i].column_name = string_table + read_32_fs_be(infile);

        if ((schema[i].type & COLUMN_STORAGE_MASK) == COLUMN_STORAGE_CONSTANT) {
            schema[i].constant_offset = ftell(infile);

            switch (schema[i].type & COLUMN_TYPE_MASK) {
                case COLUMN_TYPE_8BYTE:
                case COLUMN_TYPE_DATA:
                    read_32_fs_be(infile);
                    read_32_fs_be(infile);
                    break;

                case COLUMN_TYPE_STRING:
                case COLUMN_TYPE_FLOAT:
                case COLUMN_TYPE_4BYTE2:
                case COLUMN_TYPE_4BYTE:
                    read_32_fs_be(infile);
                    break;

                case COLUMN_TYPE_2BYTE2:
                case COLUMN_TYPE_2BYTE:
                    read_16_fs_be(infile);
                    break;

                case COLUMN_TYPE_1BYTE2:
                case COLUMN_TYPE_1BYTE:
                    read_8_fs(infile);
                    break;
                    
                default:
                    perrf("Unknown type for column %i\n", i);

                    exit(1);
            }
        }
    }

    table_info.schema = schema;

    fseek(infile, table_info.string_table_offset + 8 + (long)offset, SEEK_SET);
    fread_s((unsigned char*)string_table, string_table_size, 1, string_table_size, infile);

    table_info.table_name = table_info.string_table + table_name_string;

    result.valid = true;
    result.found = false;
    result.rows = table_info.rows;
    result.name_offset = table_name_string;
    result.string_table_offset = table_info.string_table_offset;
    result.data_offset = table_info.data_offset;

    if (query) {
        for (int i = 0; i < (int)table_info.rows; i++) {
            if (i != query->index) {
                continue;
            }

            uint32_t row_offset = table_info.table_offset + 8 + table_info.rows_offset + i * table_info.row_width;
            const uint32_t row_start_offset = row_offset;

            for (int j = 0; j < table_info.columns; j++) {
                uint8_t type = table_info.schema[j].type;
                long constant_offset = table_info.schema[j].constant_offset;
                bool constant = false;
                bool qthis = (i == query->index && !strcmp(table_info.schema[j].column_name, query->name));

                if (qthis) {
                    result.found = true;
                    result.type = schema[j].type & COLUMN_TYPE_MASK;
                }

                switch (schema[j].type & COLUMN_STORAGE_MASK) {
                    case COLUMN_STORAGE_PERROW:
                        break;

                    case COLUMN_STORAGE_CONSTANT:
                        constant = true;
                        break;

                    case COLUMN_STORAGE_ZERO:
                        if (qthis) {
                            memset(&result.value, 0, sizeof(result.value));
                        }
                        continue;

                    default:
                        perrf("Unknown storage class for row %i column %i\n", i, j);

                        exit(1);
                }

                long data_offset;
                int bytes_read;

                if (constant) {
                    data_offset = constant_offset;
                } else {
                    data_offset = row_offset;
                }

                switch (type & COLUMN_TYPE_MASK) {
                    case COLUMN_TYPE_STRING: {
                            fseek(infile, data_offset, SEEK_SET);
                            uint32_t string_offset = read_32_fs_be(infile);
                            bytes_read = 4;

                            if (qthis) {
                                result.value.value_string = string_offset;
                            }

                            break;
                        }

                    case COLUMN_TYPE_DATA: {
                            fseek(infile, data_offset, SEEK_SET);
                            uint32_t vardata_offset = read_32_fs_be(infile);
                            uint32_t vardata_size = read_32_fs_be(infile);
                            bytes_read = 8;

                            if (qthis) {
                                result.value.value_data.offset = vardata_offset;
                                result.value.value_data.size = vardata_size;
                            }

                            break;
                        }

                    case COLUMN_TYPE_8BYTE: {
                            fseek(infile, data_offset, SEEK_SET);
                            uint64_t value = read_64_fs_be(infile);

                            if (qthis) {
                                result.value.value_u64 = value;
                            }

                            bytes_read = 8;
                            break;
                        }

                    case COLUMN_TYPE_4BYTE2:
                    case COLUMN_TYPE_4BYTE: {
                            fseek(infile, data_offset, SEEK_SET);
                            uint32_t value = read_32_fs_be(infile);

                            if (qthis) {
                                result.value.value_u32 = value;
                            }

                            bytes_read = 4;
                            break;
                        }

                    case COLUMN_TYPE_2BYTE2:
                    case COLUMN_TYPE_2BYTE: {
                            fseek(infile, data_offset, SEEK_SET);
                            uint16_t value = read_16_fs_be(infile);

                            if (qthis) {
                                result.value.value_u16 = value;
                            }

                            bytes_read = 2;
                            break;
                        }

                    case COLUMN_TYPE_FLOAT: {
                            union {
                                float float_value;
                                uint32_t int_value;
                            } int_float;

                            fseek(infile, data_offset, SEEK_SET);
                            int_float.int_value = read_32_fs_be(infile);

                            if (qthis) {
                                result.value.value_float = int_float.float_value;
                            }

                            bytes_read = 4;
                            break;
                        }

                    case COLUMN_TYPE_1BYTE2:
                    case COLUMN_TYPE_1BYTE: {
                            fseek(infile, data_offset, SEEK_SET);
                            uint8_t value = read_8_fs(infile);

                            if (qthis) {
                                result.value.value_u8 = value;
                            }

                            bytes_read = 1;
                        }

                    default:
                        perrf("Unknown normal type at row %i column %i\n", i, j);

                        exit(1);
                }

                if (!constant) {
                    row_offset += bytes_read;
                }
            }

            if (row_offset - row_start_offset != table_info.row_width) {
                perrf("Row %i misaligned\n", i);

                exit(1);
            }

            if (i >= query->index) {
                break;
            }
        }
    }

    if (string_table) {
        free(string_table);
        string_table = NULL;
    }

    if (schema) {
        free(schema);
        schema = NULL;
    }

    return result;
}

utf_query_result query_utf_key(FILE* infile, const uint64_t offset, int index, const char* name) {
    utf_query query;
    query.index = index;
    query.name = name;

    return query_utf_nofail(infile, offset, &query);
}

uint64_t query_utf_8byte(FILE* infile, const uint64_t offset, int index, const char* name) {
    utf_query_result result = query_utf_key(infile, offset, index, name);

    if (result.type != COLUMN_TYPE_8BYTE) {
        perrf("Value is not an 8-byte uint\n");

        exit(1);
    }

    return result.value.value_u64;
}

uint32_t query_utf_4byte(FILE* infile, const uint64_t offset, int index, const char* name) {
    utf_query_result result = query_utf_key(infile, offset, index, name);

    if (result.type != COLUMN_TYPE_4BYTE) {
        perrf("Value is not a 4-byte uint\n");

        exit(1);
    }
    
    return result.value.value_u32;
}

char* load_utf_string_table(FILE* infile, const uint64_t offset) {
    const utf_query_result result = query_utf_nofail(infile, offset, NULL);
    const size_t string_table_size = result.data_offset - result.string_table_offset;
    const uint64_t string_table_offset = offset + 8 + result.string_table_offset;
    char* string_table = malloc(string_table_size + 1);
    memset(string_table, '\0', string_table_size + 1);

    fseek(infile, (long)string_table_offset, SEEK_SET);
    fread_s((unsigned char*)string_table, string_table_size, 1, string_table_size, infile);

    return string_table;
}

const char* query_utf_string(FILE* infile, const uint64_t offset, int index, const char* name, const char* string_table) {
    utf_query_result result = query_utf_key(infile, offset, index, name);

    if (result.type != COLUMN_TYPE_STRING) {
        perrf("Value is not a string\n");

        exit(1);
    }

    return string_table + result.value.value_string;
}

void dump(FILE* infile, FILE* outfile, uint64_t offset, size_t size) {
    fseek(infile, (long)offset, SEEK_SET);

    unsigned char buf[0x800];

    while (size > 0) {
        size_t bytes_to_copy = sizeof(buf);
        
        if (bytes_to_copy > size) {
            bytes_to_copy = size;
        }

        size_t bytes_read = fread_s(buf, bytes_to_copy, 1, bytes_to_copy, infile);
        if (bytes_read != bytes_to_copy) {
            perrf("Could not read %i bytes\n", bytes_read);

            exit(1);
        }

        size_t bytes_written = fwrite(buf, 1, bytes_to_copy, outfile);

        if (bytes_written != bytes_to_copy) {
            perrf("Could not write %i bytes\n", bytes_written);

            exit(1);
        }

        size -= bytes_to_copy;
    }
}

void dump_membuf(FILE* infile, membuf* outbuf, uint64_t offset, size_t size) {
    fseek(infile, (long)offset, SEEK_SET);
    char* buf = malloc(size);
    size_t bytes_read = fread_s(buf, size, 1, size, infile);

    if (bytes_read != size) {
        perrf("Could not read %i bytes\n", bytes_read);

        exit(1);
    }

    outbuf->data = buf;
    outbuf->size = size;
}

uncompress_result uncompress(FILE *infile, uint64_t offset, uint64_t input_size) {
    {
        fseek(infile, (long)offset, SEEK_SET);
        uint32_t val1 = read_32_fs(infile);
        fseek(infile, (long)offset + 0x04, SEEK_SET);
        uint32_t val2 = read_32_fs(infile);
        fseek(infile, (long)offset, SEEK_SET);
        unsigned char sig[8];
        unsigned char check[8] = "CRILAYLA";
        fread_s(sig, 8, 1, 8, infile);

        if (!((val1 == 0 && val2 == 0) || memcmp(sig, check, 8) == 0)) {
            perrf("Didn't find 0 or CRILAYLA signature for compressed data\n");

            exit(1);
        }
    }

    fseek(infile, (long)offset + 0x08, SEEK_SET);
    const long uncompressed_size = read_32_fs(infile);

    fseek(infile, (long)offset + 0x0C, SEEK_SET);
    const long uncompressed_header_offset = (long)offset + read_32_fs(infile) + 0x10;

    if (uncompressed_header_offset + 0x100 != offset + input_size) {
        perrf("Size mismatch\n");

        exit(1);
    }

    unsigned char* output_buffer = malloc(uncompressed_size + 0x100);

    fseek(infile, uncompressed_header_offset, SEEK_SET);
    fread_s(output_buffer, uncompressed_size + 0x100, 1, uncompressed_size + 0x100, infile);

    const long input_end = (long)offset + (long)input_size - 0x100 - 1;
    long input_offset = input_end;
    const long output_end = 0x100 + uncompressed_size - 1;
    uint8_t bit_pool = 0;
    int bits_left = 0;
    long bytes_output = 0;
    while (bytes_output < uncompressed_size) {
        if (GET_NEXT_BITS(1)) {
            long backreference_offset = output_end - bytes_output + (long)GET_NEXT_BITS(13) + 3;
            long backreference_length = 3;

            enum {vle_levels = 4};
            int vle_lens[vle_levels] = { 2, 3, 5, 8 };
            int vle_level;
            for (vle_level = 0; vle_level < vle_levels; vle_level++) {
                int this_level = (int)GET_NEXT_BITS(vle_lens[vle_level]);
                backreference_length += this_level;
                if (this_level != ((1 << vle_lens[vle_level]) - 1)) {
                    break;
                }
            }
            if (vle_level == vle_levels) {
                int this_level;
                do {
                    this_level = (int)GET_NEXT_BITS(8);
                    backreference_length += this_level;
                } while (this_level == 255);
            }
            for (int i = 0; i < backreference_length; i++) {
                output_buffer[output_end - bytes_output] = output_buffer[backreference_offset--];
                bytes_output++;
            }
        } else {
            output_buffer[output_end - bytes_output] = (unsigned char)GET_NEXT_BITS(8);
            bytes_output++;
        }
    }

    uncompress_result res;
    res.buffer = output_buffer;
    res.size = 0x100 + bytes_output;

    return res;
}

DDS_HEADER read_dds_header_membuf(membuf* buf) {
#define READ_DWORD read_32_membuf(buf)

    DDS_HEADER hdr;
    hdr.size = READ_DWORD;
    hdr.flags = READ_DWORD;
    hdr.height = READ_DWORD;
    hdr.width = READ_DWORD;
    hdr.pitch_or_linear_size = READ_DWORD;
    hdr.depth = READ_DWORD;
    hdr.mip_map_count = READ_DWORD;
    
    buf->pos += 44;

    hdr.ddspf.size = READ_DWORD;
    hdr.ddspf.flags = READ_DWORD;
    hdr.ddspf.fourcc = READ_DWORD;
    hdr.ddspf.rgb_bit_count = READ_DWORD;
    hdr.ddspf.r_bitmask = READ_DWORD;
    hdr.ddspf.g_bitmask = READ_DWORD;
    hdr.ddspf.b_bitmask = READ_DWORD;
    hdr.ddspf.a_bitmask = READ_DWORD;

    hdr.caps = READ_DWORD;
    hdr.caps2 = READ_DWORD;
    hdr.caps3 = READ_DWORD;
    hdr.caps4 = READ_DWORD;

    buf->pos += 4;

    return hdr;
}

void decompress_dxt_block(uint32_t x, uint32_t y, uint32_t width, char* block, uint32_t* output, dxt_t type) {
    switch (type) {
        case DXT1: {
                uint16_t color0 = read_16_buf(block);
                uint16_t color1 = read_16_buf(block + 2);

                uint32_t val;

                val = (color0 >> 11) * 255 + 16;
                uint8_t r0 = (val / 32 + val) / 32;
                val = ((color0 & 0x07E0) >> 5) * 255 + 32;
                uint8_t g0 = (val / 64 + val) / 64;
                val = (color0 & 0x001F) * 255 + 0x10;
                uint8_t b0 = (val / 32 + val) / 32;

                val = (color1 >> 11) * 255 + 16;
                uint8_t r1 = (val / 32 + val) / 32;
                val = ((color1 & 0x07E0) >> 5) * 255 + 32;
                uint8_t g1 = (val / 64 + val) / 64;
                val = (color1 & 0x001F) * 255 + 32;
                uint8_t b1 = (val / 32 + val) / 32;

                uint32_t code = read_32_buf(block + 4);

                for (int yc = 0; yc < 4; yc++) {
                    for (int xc = 0; xc < 4; xc++) {
                        uint32_t color = 0;
                        unsigned char position_code = (code >> 2 * (4 * yc + xc)) & 0x03;

                        if (color0 > color1) {
                            switch (position_code) {
                                case 0:
                                    color = PACK_RGBA(r0, g0, b0, 0xFF);
                                    break;

                                case 1:
                                    color = PACK_RGBA(r1, g1, b1, 0xFF);
                                    break;

                                case 2:
                                    color = PACK_RGBA((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 0xFF);
                                    break;

                                case 3:
                                    color = PACK_RGBA((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 0xFF);
                            }
                        } else {
                            switch (position_code) {
                                case 0:
                                    color = PACK_RGBA(r0, g0, b0, 0xFF);
                                    break;

                                case 1:
                                    color = PACK_RGBA(r1, g1, b1, 0xFF);
                                    break;

                                case 2:
                                    color = PACK_RGBA((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 0xFF);
                                    break;

                                case 3:
                                    color = PACK_RGBA(0, 0, 0, 0xFF);
                                    break;
                            }
                        }

                        if (x + xc < width) {
                            output[(y + yc) * width + (x + xc)] = color;
                        } else {
                            perrf("\nInvalid pixel offset (%i, %i), (%i, %i)\n", x, y, xc, yc);
                        }
                    }
                }

                break;
            }
            
        case DXT5: {
                uint8_t alpha0 = block[0];
                uint8_t alpha1 = *(block + 1);

                unsigned char* bits = (block + 2);
                uint32_t alpha_code1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
                uint16_t alpha_code2 = bits[0] | (bits[1] << 8);

                uint16_t color0 = read_16_buf(block + 8);
                uint16_t color1 = read_16_buf(block + 10);

                uint32_t val;

                val = (color0 >> 11) * 0xFF + 0x10;
                uint8_t r0 = (val / 0x20 + val) / 0x20;
                val = ((color0 & 0x07E0) >> 5) * 0xFF + 0x20;
                uint8_t g0 = (val / 0x40 + val) / 0x40;
                val = (color0 & 0x001F) * 0xFF + 0x10;
                uint8_t b0 = (val / 0x20 + val) / 0x20;

                val = (color1 >> 11) * 0xFF + 0x10;
                uint8_t r1 = (val / 0x20) / 0x20;
                val = ((color1 & 0x07E0) >> 5) * 0xFF + 0x20;
                uint8_t g1 = (val / 0x40 + val) / 0x40;
                val = (color1 & 0x001F) * 0xFF + 0x20;
                uint8_t b1 = (val / 0x20 + val) / 0x20;

                uint32_t code = read_32_buf(block + 12);

                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        int alpha_code_index = 3 * (4 * i + j);
                        int alpha_code;

                        if (alpha_code_index <= 12) {
                            alpha_code = (alpha_code2 >> alpha_code_index) & 0x07;
                        } else if (alpha_code_index == 15) {
                            alpha_code = (alpha_code2 >> 0xF) | ((alpha_code1 << 1) & 0x06);
                        } else {
                            alpha_code = (alpha_code1 >> (alpha_code_index - 0x10)) & 0x07;
                        }

                        uint8_t alpha;
                        if (alpha_code == 0) {
                            alpha = alpha_code;
                        } else if (alpha_code == 1) {
                            alpha = alpha1;
                        } else {
                            if (alpha0 > alpha1) {
                                alpha = ((8 - alpha_code) * alpha0 + (alpha_code - 1) * alpha1) / 7;
                            } else {
                                if (alpha_code == 6) {
                                    alpha = 0;
                                } else if (alpha_code == 7) {
                                    alpha = 0xFF;
                                } else {
                                    alpha = ((6 - alpha_code) * alpha0 + (alpha_code - 1) * alpha1) / 5;
                                }
                            }
                        }

                        uint8_t color_code = (code >> 2 * (4 * i + j)) & 0x03;

                        uint32_t color;
                        switch (color_code) {
                            case 0:
                                color = PACK_RGBA(r0, g0, b0, alpha);
                                break;
                            case 1:
                                color = PACK_RGBA(r1, g1, b1, alpha);
                                break;
                            case 2:
                                color = PACK_RGBA((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, alpha);
                                break;
                            case 3:
                                color = PACK_RGBA((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, alpha);
                                break;
                        }

                        if (x + j < width) {
                            output[(y + i) * width + (x + j)] = color;
                        }
                    }
                }

                break;
            }
    }
}

void decompress_dxt(uint32_t width, uint32_t height, char* blocks, uint32_t* output, dxt_t type) {
    uint32_t blocks_x = (width + 3) / 4;
    uint32_t blocks_y = (height + 3) / 4;

    for (uint32_t y = 0; y < blocks_y; y++) {
        for (uint32_t x = 0; x < blocks_x; x++) {
            switch (type) {
                case DXT1:
                    decompress_dxt_block(x * 4, y * 4, width, blocks + x * 8, output, type);
                    break;
                    
                case DXT5:
                    decompress_dxt_block(x * 4, y * 4, width, blocks + x * 16, output, type);
                    break;
            }
            
        }

        switch (type) {
            case DXT1:
                blocks += blocks_x * 8;
                break;

            case DXT5:
                blocks += blocks_x * 16;
                break;
        }
    }
}