#include "USMPlayer.h"

#include "BitManipulation.h"
#include "Error.h"

USMPlayer::USMPlayer(const char* fname, std::map<uint32_t, QIcon>& icons, QWidget* parent) : QWidget(parent), CripackReader(QFileInfo(fname), icons, false),
    infile(fname, std::ios::binary),
    layout(new QGridLayout(this)),
    video_widget(new QVideoWidget(this)),
    player(new QMediaPlayer()),
    fname(fname) {
    this->setLayout(layout);

    layout->addWidget(video_widget, 0, 0);

    player->setVideoOutput(video_widget);

    infile.seekg(0, std::ios::end);

    fsize = infile.tellg();

    infile.seekg(0, std::ios::beg);

    analyse();
}

void USMPlayer::analyse() {
    uint64_t n_streams = 0;
    StreamInfo* streams = nullptr;
    char* CRIUSF_strtbl = nullptr;
    int32_t live_streams = 0;
    bool streams_setup = false;

    enum {
        CRID = 0x43524944,
        SFV = 0x40534656,
        SFA = 0x40534641
    };

    do {
        uint32_t stdmid = read_32_be(infile);
        uint32_t block_size = read_32_be(infile);
        uint32_t block_type;
        uint16_t header_size;
        uint16_t footer_size;
        uint64_t payload_bytes;
        uint32_t stream_idx;

        if (!streams_setup) {
            if (stdmid != CRID) {
                throw USMFormatError("Invalid CRID header");
            }
        } else {
            if (n_streams == 0) {
                throw USMFormatError("Invalid number of streams");
            }

            for (stream_idx = 1; stream_idx < n_streams; stream_idx++) {
                if (stdmid == streams[stream_idx].stmid) {
                    break;
                }
            }

            if (stream_idx == n_streams) {
                throw USMFormatError("Unknown stdmid");
            }

            if (!streams[stream_idx].alive) {
                throw USMFormatError("Stream should be at end");
            }

            streams[stream_idx].bytes_read += block_size;
        }

        {
            header_size = read_16_be(infile);
            footer_size = read_16_be(infile);

            if (header_size != 0x18) {
                throw USMFormatError("Expected header size of 24");
            }

            payload_bytes = block_size - header_size - footer_size;
        }

        block_type = read_32_be(infile);

        {
            uint32_t int0 = read_32_be(infile);
            uint32_t int1 = read_32_be(infile);
            uint32_t int2 = read_32_be(infile);
            uint32_t int3 = read_32_be(infile);

            if (int2 != 0 && int3 != 0) {
                throw USMFormatError("Unknown byte format");
            }
        }

        if (!streams_setup) {
            int64_t CRIUSF_offset = infile.tellg();

            if (block_type != 1) {
                throw USMFormatError("CRID type should be 1");
            }

            {
                this->read_utf();
                UTF* utf = new UTF(reinterpret_cast<unsigned char*>(utf_packet));

                if (utf->n_rows < 1) {
                    throw USMFormatError("Expected at least 1 row");
                }

                n_streams = utf->n_rows;

                uint64_t CRIUSF_data_offset = CRIUSF_offset + utf->get_data_offset();
                CRIUSF_strtbl = utf->load_strtbl(infile, CRIUSF_offset);
                
                if (strcmp(&CRIUSF_strtbl[utf->get_table_name()], "CRIUSF_DIR_STREAM") != 0) {
                    throw USMFormatError("Expected CRIUSF_DIR_STREAM");
                }
            }
        }

    } while (live_streams > 0);
}