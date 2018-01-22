#pragma once

#include "Error.h"

#include <qfileinfo.h>
#include <qstandarditemmodel.h>
#include <qfileiconprovider.h>

#include <fstream>
#include <iostream>

#define C_STR(s) s.toStdString().c_str()

class CripackReader {
    public:
        explicit CripackReader(QFileInfo file);
        ~CripackReader();

        //std::vector<QStandardItem*> file_contents(std::map<uint32_t, QIcon> file_icons);
        //std::vector<std::string> tocs();

    private:
        //std::map<uint32_t, QIcon> file_icons;
        typedef union UTFRowValues {
            uint8_t  v8;
            uint16_t v16;
            uint32_t v32;
            uint64_t v64;
            char*    data;
            float    vfloat;
            std::string vstring;
        } UTFRowValues;

        struct EmbeddedFile {
            EmbeddedFile(std::string file_name, uint64_t file_offset, uint64_t file_offset_pos, std::string toc_name, std::string file_type) :
                file_name(file_name),
                file_offset(file_offset),
                file_offset_pos(file_offset_pos),
                toc_name(toc_name),
                file_type(file_type) { }

            std::string dir_name;
            std::string file_name;
            std::string file_type;
            std::string toc_name;

            uint64_t file_size;
            uint64_t file_size_pos;

            uint64_t extract_size;
            uint64_t extract_size_pos;

            uint64_t file_offset;
            uint64_t file_offset_pos;

            uint64_t offset;
        };

        struct UTFColumn {
            char flags;
            std::string name;
        };

        struct UTFRow {
            int32_t type = -1;
            uint64_t position = 0;
            
            UTFRowValues val;
        };

        struct UTFRows {
            std::vector<UTFRow> rows;
        };

        class UTF {
            public:
                explicit UTF(unsigned char* utf_packet);
                ~UTF() { }

                std::vector<UTFColumn> columns;
                std::vector<UTFRows> rows;

                uint16_t n_columns;

            private:
                unsigned char* utf_packet;
                uint64_t pos;

                uint32_t table_size;
                uint64_t rows_offset;
                uint64_t strings_offset;
                uint64_t data_offset;
                uint32_t table_name;
                uint16_t row_length;
                uint32_t n_rows;

                enum StorageFlags : uint32_t {
                    STORAGE_MASK = 0xf0,
                    STORAGE_NONE = 0x00,
                    STORAGE_ZERO = 0x10,
                    STORAGE_CONSTANT = 0x30,
                    STORAGE_PERROW = 0x50
                };

                enum TypeFlags : uint32_t {
                    TYPE_MASK = 0xf,
                    TYPE_DATA = 0x0b,
                    TYPE_STRING = 0x0a,
                    TYPE_FLOAT = 0x08,
                    TYPE_8BYTE2 = 0x07,
                    TYPE_8BYTE = 0x06,
                    TYPE_4BYTE2 = 0x05,
                    TYPE_4BYTE = 0x04,
                    TYPE_2BYTE2 = 0x03,
                    TYPE_2BYTE = 0x02,
                    TYPE_1BYTE2 = 0x01,
                    TYPE_1BYTE = 0x00
                };
        };

        std::ifstream infile;
        std::vector<EmbeddedFile> file_table;
        std::unordered_map<std::string, UTFRowValues> cpkdata;

        uint32_t unk1;
        uint64_t utf_size;
        char* utf_packet;
        char* cpk_packet;

        uint64_t TocOffset;
        uint64_t EtocOffset;
        uint64_t ItocOffset;
        uint64_t GtocOffset;
        uint64_t ContentOffset;

        UTF* utf;

        UTFRowValues get_column_data(uint32_t row, std::string name);
        uint64_t get_column_position(row, std::string name);
        /*uint64_t toc_entries;
        char* toc_strtbl;
        uint64_t toc_offset;

        typedef std::tuple<std::string, std::string, uint32_t, uint32_t> file_tuple;

        typedef struct query_result {
            bool valid = false;
            bool found = false;
            int type;

            union {
                uint64_t vu64;
                uint32_t vu32;
                uint16_t vu16;
                uint8_t  vu8;
                float    vfloat;
                uint32_t vstring;
                struct vdata {
                    uint64_t offset;
                    uint64_t size;
                } vdata;
            } value;

            uint32_t rows;
            uint32_t name_offset;
            uint32_t strtbl_offset;
            uint32_t data_offset;
        } query_result;

        typedef struct query {
            const char* name;
            uint64_t    index;
        } query;

        typedef struct column {
            uint8_t type;
            const char* column_name;
            uint64_t const_offset;
        } column;

        typedef struct table {
            uint64_t offset;
            uint32_t size;
            uint32_t schema_offset;
            uint32_t rows_offset;
            uint32_t strtbl_offset;
            uint32_t data_offset;
            const char* strtbl;
            const char* tbl_name;
            uint16_t columns;
            uint16_t row_width;
            uint32_t rows;

            std::vector<column> schema;
        } table;

        const enum {
            STORAGE_MASK     = 0xf0,
            STORAGE_PERROW   = 0x50,
            STORAGE_CONSTANT = 0x30,
            STORAGE_ZERO     = 0x10
        };

        const enum {
            TYPE_MASK   = 0x0f,
            TYPE_DATA   = 0x0b,
            TYPE_STRING = 0x0a,
            TYPE_FLOAT  = 0x08,
            TYPE_8BYTE  = 0x06,
            TYPE_4BYTE2 = 0x05,
            TYPE_4BYTE  = 0x04,
            TYPE_2BYTE2 = 0x03,
            TYPE_2BYTE  = 0x02,
            TYPE_1BYTE2 = 0x01,
            TYPE_1BYTE  = 0x00
        };

        query_result query_utf(uint64_t offset, query* query);

        query_result query_key(uint64_t offset, uint32_t index, const char* name) {
            query q;
            q.index = index;
            q.name  = name;

            return query_utf(offset, &q);
        }

        inline uint64_t query_8(uint64_t offset, uint32_t index, const char* name) {
            query_result result = query_key(offset, index, name);

            if (result.type != TYPE_8BYTE) {
                throw FormatError("Value is not an 8-byte uint");
            }

            return result.value.vu64;
        }

        uint32_t query_4(uint64_t offset, uint32_t index, const char* name) {
            query_result result = query_key(offset, index, name);

            if (result.type != TYPE_4BYTE) {
                throw FormatError("Value is not a 4-byte uint");
            }

            return result.value.vu32;
        }

        const char* query_string(uint64_t offset, uint32_t index, const char* name, const char* strtbl) {
            query_result result = query_key(offset, index, name);

            if (result.type != TYPE_STRING) {
                throw FormatError("Value is not a string");
            }

            return strtbl + result.value.vstring;
        }

        char* load_strtbl(uint64_t offset) {
            query_result result = query_utf(offset, NULL);
            size_t strtbl_size = result.data_offset - result.strtbl_offset;
            uint64_t strtbl_offset = offset + 8 + result.strtbl_offset;
            char* strtbl = new char[strtbl_size + 1]();

            infile.seekg(strtbl_offset, std::ios::beg);
            infile.read(strtbl, strtbl_size + 1);

            return strtbl;
        }

        inline bool ends_with(std::string const &v, std::string const &e) {
            if (e.size() > v.size()) return false;

            return std::equal(e.rbegin(), e.rend(), v.rbegin());
        }

        std::vector<QStandardItem*> merge_dirs(std::vector<std::string> dirs, std::vector<file_tuple> files);

        std::vector<QStandardItem*> match_dirs(std::string dir, std::vector<file_tuple> files);

        QIcon fname_to_icon(std::string fname);*/
};

Q_DECLARE_METATYPE(CripackReader*);