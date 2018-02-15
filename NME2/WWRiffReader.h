#pragma once

#include "Error.h"
#include "BitManipulation.h"

#include <QWidget>
#include <QBuffer>
#include <QFileInfo>
#include <QStandardItemModel>

#include <fstream>

class WWRiffReader : public QWidget {
    Q_OBJECT

    public:
    explicit WWRiffReader(std::string fpath, std::map<uint32_t, QIcon>& icons);
    ~WWRiffReader() {
        infile.close();
    }

    struct WWRiffFile {
        uint64_t size;
        uint64_t offset;
    };

    void set_embedded_file(WWRiffFile file);
    
    std::vector<QStandardItem*> file_contents();

    private:
    class BitStream {
        public:
        BitStream(QBuffer* input) : input(input) { }
        bool get_bit();

        uint64_t get_bits_read() const {
            return bits_read;
        }

        private:
        QBuffer* input;

        uint8_t bit_buffer = 0;
        uint64_t bits_left = 0;
        uint64_t bits_read = 0;
    };

    class VarInt;

    class OggStream {
        public:
        explicit OggStream(QBuffer* output) : output(output) { }

        void put_bit(bool bit);
        void write_packet_header(uint8_t type);
        void flush_page(bool next_continued = false, bool last = false);

        private:
        static constexpr unsigned char n_header_bytes = 27;
        static constexpr unsigned char n_max_segments = 255;
        static constexpr unsigned char n_segment_size = 255;

        QBuffer* output;
        uint8_t bit_buffer = 0;
        unsigned char page_buffer[n_header_bytes + n_max_segments + n_segment_size * n_max_segments];

        uint8_t bits_stored = 0;
        uint32_t payload_bytes = 0;

        uint32_t granule = 0;
        uint32_t seqno = 0;

        bool first = false;
        bool continued = false;

        void flush_bits();

        uint32_t checksum(unsigned char* data, uint32_t bytes);
    };

    class VarInt {
        public:
        explicit VarInt(uint64_t val, uint8_t size) : val(val), size(size) { };

        VarInt& operator = (uint64_t v) {
            val = v;
            return *this;
        }

        operator uint64_t() const {
            return val;
        }

        friend BitStream& operator >> (BitStream& stream, VarInt& vint) {
            vint.val = 0;

            for (uint8_t i = 0; i < vint.size; i++) {
                if (stream.get_bit())
                    vint.val |= (1U << i);
            }

            return stream;
        }

        friend OggStream& operator << (OggStream& stream, const VarInt& vint) {
            for (uint8_t i = 0; i < vint.size; i++) {
                stream.put_bit((vint.val & (1U << i)) != 0);
            }

            return stream;
        }

        uint64_t val;
        uint8_t size;
    };

    class VorbisPacket {
        public:
        explicit VorbisPacket(QBuffer* input, int64_t offset) : offset(offset), size(read_16_le(input)) { }

        int64_t packet_offset() {
            return offset + 2;
        }

        int64_t next_packet_offset() {
            return packet_offset() + 2 + size;
        }

        private:
        int64_t offset;
        uint16_t size;
        uint32_t granule = 0;
    };

    class CodebookLibrary {
        public:
        explicit CodebookLibrary() : data(new char[offset_offset]), offsets(new uint64_t[codebook_count]) { }

        static constexpr int32_t codebook_count = 599;
        static constexpr int32_t offset_offset = 71991;

        char* data;
        uint64_t* offsets;

        private:
        uint64_t count;
    };

    std::ifstream infile;
    QFileInfo file;
    std::map<uint32_t, QIcon>& file_icons;

    WWRiffFile current_file;
    QBuffer* audio = nullptr;

    std::vector<WWRiffFile> file_table;

    QBuffer* convert_wwriff(QBuffer* input);
};

Q_DECLARE_METATYPE(WWRiffReader*);
Q_DECLARE_METATYPE(WWRiffReader::WWRiffFile);