#pragma once

#include "Error.h"
#include "BitManipulation.h"

#include <qfileinfo.h>
#include <qstandarditemmodel.h>
#include <qfileiconprovider.h>

#include <fstream>
#include <iostream>

class CripackReader {
    public:
    explicit CripackReader(QFileInfo file, std::map<uint32_t, QIcon>& icons, bool init = true);
    ~CripackReader() {
        infile.close();
    }

    std::vector<QStandardItem*> file_contents();

    static char* decompress_crilayla(char* input, uint64_t input_size);

    struct EmbeddedFile {
        EmbeddedFile() { }
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
        std::string user_string;
        std::string local_dir;

        uint64_t file_size;
        uint64_t file_size_pos;

        uint64_t extract_size;
        uint64_t extract_size_pos;

        uint64_t file_offset;
        uint64_t file_offset_pos;

        uint64_t offset;

        uint64_t update_date_time;

        uint32_t id;
    };

    class DATReader {
        public:
        explicit DATReader(char* file, std::map<uint32_t, QIcon>& file_icons);
        ~DATReader() { }

        std::vector<QStandardItem*> file_contents();

        struct LightEmbeddedFile {
            std::string name;
            uint64_t offset;
            uint64_t size;
            uint64_t index;
        };

        private:
        std::map<uint32_t, QIcon>& file_icons;

        uint32_t file_count;
        uint32_t file_table_offset;
        uint32_t name_table_offset;
        uint32_t size_table_offset;
        uint32_t name_align;

        std::vector<LightEmbeddedFile> file_table;
    };

    protected:
    inline void read_utf() {
        unk1 = read_32_le(infile);
        utf_size = read_64_le(infile);
        utf_packet = new char[utf_size];

        infile.read(utf_packet, utf_size);
    }

    struct UTFColumn {
        char flags;
        char* name;
    };

    struct UTFRow {
        int32_t type = -1;
        uint64_t position = 0;
            
        QVariant val;
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
        uint32_t n_rows;

        inline uint64_t get_data_offset() { return data_offset; }
        inline uint32_t get_table_name() { return table_name; }

        char* load_strtbl(std::ifstream& infile, uint64_t offset);

        private:
        bool mem;

        unsigned char* utf_packet;
        uint64_t pos;

        uint32_t table_size;
        uint64_t rows_offset;
        uint64_t strings_offset;
        uint64_t data_offset;
        uint32_t table_name;
        uint16_t row_length;

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
    std::map<uint32_t, QIcon>& file_icons;

    std::vector<EmbeddedFile> file_table;
    std::map<std::string, QVariant> cpkdata;

    uint32_t unk1;
    uint64_t utf_size;

    char* utf_packet;
    char* cpk_packet;
    char* toc_packet;
    char* etoc_packet;
    char* itoc_packet;
    char* gtoc_packet;

    uint64_t TocOffset;
    uint64_t EtocOffset;
    uint64_t ItocOffset;
    uint64_t GtocOffset;
    uint64_t ContentOffset;

    uint16_t alignment;

    UTF* utf;
    UTF* files;

    QVariant get_column_data(UTF* utf_src, uint32_t row, std::string name);
    uint64_t get_column_position(UTF* utf_src, uint32_t row, std::string name);
    bool column_exists(UTF* utf_src, uint32_t row, std::string name);

    void init();
    void read_toc();
    void read_etoc();
    void read_itoc();
    void read_gtoc();

    std::vector<QStandardItem*> merge_dirs(std::vector<std::string> dirs);
    std::vector<QStandardItem*> match_dirs(std::string dir);

    static inline uint16_t get_bits(char* input, uint64_t* offset, uint8_t* bit_pool, uint64_t* bits_left, uint64_t bit_count) {
        uint16_t out_bits = 0;
        uint64_t bits_produced = 0;
        uint64_t bits_this_round = 0;

        while (bits_produced < bit_count) {
            if (*bits_left == 0) {
                *bit_pool = input[*offset];
                *bits_left = 8;
                --*offset;
            }

            if (*bits_left > (bit_count - bits_produced)) {
                bits_this_round = bit_count - bits_produced;
            } else {
                bits_this_round = *bits_left;
            }

            out_bits <<= bits_this_round;
            out_bits |= static_cast<uint16_t>(static_cast<uint16_t>(*bit_pool >> (*bits_left - bits_this_round)) & ((1 << bits_this_round) - 1));

            *bits_left -= bits_this_round;
            bits_produced += bits_this_round;
        }

        return out_bits;
    }
};

Q_DECLARE_METATYPE(CripackReader*);
Q_DECLARE_METATYPE(CripackReader::EmbeddedFile);
Q_DECLARE_METATYPE(CripackReader::DATReader*);
Q_DECLARE_METATYPE(CripackReader::DATReader::LightEmbeddedFile);