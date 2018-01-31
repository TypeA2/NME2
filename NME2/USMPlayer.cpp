#include "USMPlayer.h"

#include "BitManipulation.h"
#include "Error.h"

USMPlayer::USMPlayer(std::string fpath, std::map<uint32_t, QIcon>& icons, QWidget* parent) : QWidget(parent), CripackReader(infile, icons, false),
    infile(fpath.c_str(), std::ios::binary | std::ios::in),
    layout(new QGridLayout(this)),
    video_widget(new QVideoWidget(this)),
    player(new QMediaPlayer()),
    infile_path(fpath) {
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

    std::vector<QByteArray> stream_data;
    std::vector<QDataStream*> outstreams;

    enum {
        CRID = 0x43524944,
        SFV = 0x40534656,
        SFA = 0x40534641
    };

    do {
        uint32_t stmid = read_32_be(infile);
        uint32_t block_size = read_32_be(infile);
        uint32_t block_type;
        uint16_t header_size;
        uint16_t footer_size;
        uint64_t payload_bytes;
        uint32_t stream_idx;

        if (!streams_setup) {
            if (stmid != CRID) {
                throw USMFormatError("Invalid CRID header");
            }
        } else {
            if (n_streams == 0) {
                throw USMFormatError("Invalid number of streams");
            }

            for (stream_idx = 1; stream_idx < n_streams; stream_idx++) {
                if (stmid == streams[stream_idx].stmid) {
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
            UTF* utf;

            if (block_type != 1) {
                throw USMFormatError("CRID type should be 1");
            }

            {
                this->read_utf_noheader();
                utf = new UTF(reinterpret_cast<unsigned char*>(utf_packet), true);

                if (utf->n_rows < 1) {
                    throw USMFormatError("Expected at least 1 row");
                }

                n_streams = utf->n_rows;

                uint64_t CRIUSF_data_offset = CRIUSF_offset + utf->get_data_offset();
                CRIUSF_strtbl = utf->load_strtbl(infile, CRIUSF_offset - 8);
                
                if (strcmp(&CRIUSF_strtbl[utf->get_table_name()], "CRIUSF_DIR_STREAM") != 0) {
                    throw USMFormatError("Expected CRIUSF_DIR_STREAM");
                }
            }

            {
                streams = new StreamInfo[n_streams];

                for (uint32_t i = 0; i < n_streams; i++) {
                    StreamInfo* stream = &streams[i];

                    stream->fname = get_column_data(utf, i, "filename").toString().toStdString();
                    stream->fsize = get_column_data(utf, i, "filesize").toULongLong();
                    stream->dsize = get_column_data(utf, i, "datasize").toULongLong();
                    stream->stmid = get_column_data(utf, i, "stdmid").toUInt();
                    stream->chno = get_column_data(utf, i, "chno").toUInt();
                    stream->minchk = get_column_data(utf, i, "minchk").toUInt();
                    stream->minbuf = get_column_data(utf, i, "minbuf").toUInt();
                    stream->avbps = get_column_data(utf, i, "avbps").toUInt();

                    if (i == 0) {
                        if (stream->stmid != 0) {
                            throw USMFormatError("Expected stmid for stream #0");
                        }

                        if (stream->chno != 0xffff) {
                            throw USMFormatError("Expected chno -1");
                        }
                    } else {
                        switch (stream->stmid) {
                            case SFV:
                            case SFA:
                                break;
                            default:
                                qDebug() << utf->columns[i].name.c_str() << utf->rows[i].rows[4].val;
                                qDebug() << get_column_data(utf, i, "stdmid");
                                throw USMFormatError("Unknown stdmid");
                        }

                        if (stream->dsize != 0) {
                            throw USMFormatError("Expected datasize 0");
                        }

                        for (uint32_t j = 0; j < i; j++) {
                            if (stream->stmid == streams[j].stmid) {
                                throw USMFormatError("Duplicate stdmid");
                            }
                        }
                    }
                }
            }

            {
                std::cout << n_streams << " streams" << std::endl;

                for (uint32_t i = 0; i < n_streams; i++) {
                    if (i == 0) {
                        outstreams.push_back(nullptr);
                    } else {
                        stream_data.push_back(QByteArray());

                        switch (streams[i].stmid) {
                            case SFV:
                                std::cout << "mpeg" << std::endl;
                                break;

                            case SFA:
                                std::cout << "adx" << std::endl;
                                break;
                        }

                        outstreams.push_back(new QDataStream(stream_data[i]));
                    }
                }
            }

            {
                streams[0].alive = 0;

                for (uint32_t i = 1; i < n_streams; i++) {
                    streams[i].alive = 1;
                    live_streams++;
                    streams[i].bytes_read = 0;
                    streams[i].payload_bytes = 0;
                }
            }
            
            streams_setup = true;

            infile.seekg(CRIUSF_offset + payload_bytes, std::ios::beg);
        } else {
        switch (block_type) {
            case 0: { // Data
                    char* payload = new char[payload_bytes];

                    infile.read(payload, payload_bytes);
                    outstreams[stream_idx]->writeRawData(payload, payload_bytes);

                    delete payload;

                    break;
                }
                
            case 1: // Header
            case 3: // Metadata
                infile.seekg(payload_bytes, std::ios::cur);
                break;

            case 2: { // Stream metadata
                    char* metadata = new char[payload_bytes];

                    infile.read(metadata, payload_bytes);

                    if (strncmp(metadata, "#CONTENTS END   ===============", payload_bytes) == 0) {
                        streams[stream_idx].alive = 0;
                        live_streams--;
                    }

                    delete metadata;
                    
                    break;
                }

            default:
                throw USMFormatError("Unknown block type");
            }
        }

        {
            for (uint32_t i = 0; i < footer_size; i++) {
                char byte;

                infile.read(&byte, 1);

                if (byte != '\0') {
                    throw USMFormatError("Nonzero padding");
                }
            }
        }

    } while (live_streams > 0);

    if (!streams_setup) {
        throw USMFormatError("No CRID found");
    }
}