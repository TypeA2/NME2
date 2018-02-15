#include "WWRiffReader.h"

#include "NME2.h"

WWRiffReader::WWRiffReader(std::string fpath, std::map<uint32_t, QIcon>& icons) : QWidget(),
    infile(fpath, std::ios::binary | std::ios::in),
    file(fpath.c_str()),
    file_icons(icons) {

    while (!infile.eof())  {
        WWRiffFile f;

        f.offset = infile.tellg();

        {
            char riff_hdr[4];
            infile.read(riff_hdr, 4);

            if (memcmp(riff_hdr, "RIFF", 4) != 0) {
                throw FormatError("Invalid RIFF signature");
            }

            f.size = read_32_le(infile) + 8;
        }

        infile.seekg(f.size - 8,  std::ios_base::cur);

        infile.ignore(std::numeric_limits<std::streamsize>::max(), 'R');

        if (!infile.eof()) {
            int64_t tmp = infile.tellg();
            infile.seekg(tmp - 1, std::ios::beg);
        }

        file_table.push_back(f);
    }

    infile.clear();
}

void WWRiffReader::set_embedded_file(WWRiffFile file) {
    current_file = file;

    if (audio != nullptr) {
        delete audio;
    }

    infile.seekg(current_file.offset, std::ios::beg);

    char* fdata = new char[current_file.size];

    infile.read(fdata, current_file.size);

    QBuffer* input = new QBuffer();
    input->open(QIODevice::ReadWrite);
    input->write(fdata, current_file.size);
    input->seek(0);

    try {
        audio = convert_wwriff(input);
    } catch (const WWRiffFormatError& e) {
        std::cout << "WWRiff conversion error: " << e.what() << std::endl;
    }
}

QBuffer* WWRiffReader::convert_wwriff(QBuffer* input) {
    QBuffer* ogg = new QBuffer();
    ogg->open(QIODevice::ReadWrite);

    int32_t riff_size = -1;

    {
        char riff_hdr[4];
        char wave_hdr[4];

        input->read(riff_hdr, sizeof(riff_hdr));
        
        if (memcmp(riff_hdr, "RIFF", sizeof(riff_hdr)) != 0) {
            throw WWRiffFormatError("Invalid RIFF header");
        }

        riff_size = (read_32_le(input) + 8);

        if (riff_size != input->size()) {
            throw WWRiffFormatError("RIFF size mismatch");
        }

        input->read(wave_hdr, sizeof(wave_hdr));

        if (memcmp(wave_hdr, "WAVE", sizeof(wave_hdr)) != 0) {
            throw WWRiffFormatError("Invalid WAVE header");
        }
    }

    int32_t chunk_offset = 12;

    int32_t fmt_offset = -1;
    int32_t fmt_size = -1;
    
    int32_t cue_offset = -1;
    int32_t cue_size = -1;

    int32_t list_offset = -1;
    int32_t list_size = -1;

    int32_t smpl_offset = -1;
    int32_t smpl_size = -1;

    int32_t vorb_offset = -1;
    int32_t vorb_size = -1;

    int32_t data_offset = -1;
    int32_t data_size = -1;

    while (chunk_offset < riff_size) {
        input->seek(chunk_offset);

        if (chunk_offset + 8 > riff_size) {
            throw WWRiffFormatError("Chunk truncated");
        }

        std::string chunk_type = input->read(4).toStdString();

        uint32_t chunk_size = read_32_le(input);

        if (chunk_type == "fmt ") {
            fmt_offset = chunk_offset + 8;
            fmt_size = chunk_size;
        } else if (chunk_type == "cue ") {
            cue_offset = chunk_offset + 8;
            cue_size = chunk_size;
        } else if (chunk_type == "LIST") {
            list_offset = chunk_offset + 8;
            list_size = chunk_size;
        } else if (chunk_type == "smpl") {
            smpl_offset = chunk_offset + 8;
            smpl_size = chunk_size;
        } else if (chunk_type == "vorb") {
            vorb_offset = chunk_offset + 8;
            vorb_size = chunk_size;
        } else if (chunk_type == "data") {
            data_offset = chunk_offset + 8;
            data_size = chunk_size;
        }

        chunk_offset = 8 + chunk_offset + chunk_size;
    }

    if (chunk_offset > riff_size) {
        throw WWRiffFormatError("Chunk truncated");
    }

    if (fmt_offset == -1 || data_offset == -1) {
        throw WWRiffFormatError("fmt and data chunks are required");
    }

    if (vorb_offset == -1 && fmt_size != 0x42) {
        throw WWRiffFormatError("fmt should have size 0x42 if vorb missing");
    }

    if (vorb_offset != -1 && fmt_size != 0x28 && fmt_size != 0x18 && fmt_size != 0x12) {
        throw WWRiffFormatError("Invalid fmt size");
    }

    if (vorb_offset == -1 && fmt_size == 0x42) {
        vorb_offset = fmt_offset + 0x18;
    }

    input->seek(fmt_offset);

    if (read_16_le(input) != 0xFFFF) {
        throw WWRiffFormatError("Invalid codec identifier");
    }

    uint16_t n_channels = read_16_le(input);
    uint32_t sample_rate = read_32_le(input);
    uint32_t avgbps = read_32_le(input);

    if (read_16_le(input) != 0U) {
        throw WWRiffFormatError("Invalid block alignment");
    }

    if (read_16_le(input) != 0U) {
        throw WWRiffFormatError("0bps is required");
    }

    if (fmt_size - 0x12 != read_16_le(input)) {
        throw WWRiffFormatError("Bad extra format length");
    }

    uint16_t ext_unk = 0;
    uint32_t subtype = 0;

    if (fmt_size - 0x12 >= 2) {
        ext_unk = read_16_le(input);

        if (fmt_size - 0x12 >= 6) {
            subtype = read_32_le(input);
        }
    }

    if (fmt_size == 0x28) {
        char buf[16];
        const uint8_t buf_check[16] = {
            1,    0,    0,    0,
            0,    0,    0x10, 0,
            0x80, 0,    0,    0xAA,
            0,    0x38, 0x9B, 0x71
        };

        input->read(buf, 16);

        if (memcmp(buf, buf_check, sizeof(buf_check)) != 0) {
            throw WWRiffFormatError("Invalid fmt signature");
        }
    }

    uint32_t cue_count = 0;
    if (cue_offset != 0) {
        input->seek(cue_offset);
        cue_count = read_32_le(input);
    }

    uint32_t loop_count = 0;
    uint32_t loop_start = 0;
    uint32_t loop_end = 0;
    if (smpl_offset != -1) {
        input->seek(smpl_offset + 0x1C);
        loop_count = read_32_le(input);

        if (loop_count != 1) {
            throw WWRiffFormatError("Expected 1 loop");
        }

        input->seek(smpl_offset + 0x2C);

        loop_start = read_32_le(input);
        loop_end = read_32_le(input);
    }

    switch (vorb_size) {
        case -1:
        case 0x28:
        case 0x2A:
        case 0x2C:
        case 0x32:
        case 0x34:
            input->seek(vorb_offset);
            break;

        default:
            throw WWRiffFormatError("Bad vorb size");
    }

    uint32_t sample_count = read_32_le(input);

    input->seek(vorb_offset + 0x10);

    uint32_t setup_packet_offset = read_32_le(input);
    uint32_t first_audio_packet_offset = read_32_le(input);

    switch (vorb_size) {
        case -1:
        case 0x2A:
            input->seek(vorb_offset + 0x24);
            break;

        case 0x32:
        case 0x34:
            input->seek(vorb_offset + 0x2C);
            break;
    }

    uint32_t uid = 0;
    uint8_t bsize_0_pow = 0;
    uint8_t bsize_1_pow = 0;

    switch (vorb_size) {
        case -1:
        case 0x2A:
        case 0x32:
        case 0x34:
            uid = read_32_le(input);
            bsize_0_pow = *input->read(1).data();
            bsize_1_pow = *input->read(1).data();
            break;
    }

    if (loop_count != 0) {
        if (loop_end == 0) {
            loop_end = sample_count;
        } else {
            loop_end++;
        }

        if (loop_start >= sample_count || loop_end > sample_count || loop_start > loop_end) {
            throw WWRiffFormatError("Loop out of range");
        }
    }

    OggStream os(ogg);

    // ID packet
    {
        os.write_packet_header(1);

        os << VarInt(0, 32);            // Version
        os << VarInt(n_channels, 8);    // Number of channels
        os << VarInt(sample_rate, 32);  // Sample rate
        os << VarInt(0, 32);            // Maximum bitrate
        os << VarInt(avgbps * 8, 32);   // Nominal bitrate
        os << VarInt(0, 32);           // Minimum bitrate
        os << VarInt(bsize_0_pow, 4);   // Blocksize 0
        os << VarInt(bsize_1_pow, 4);   // Blocksize 1
        os << VarInt(1, 1);             // Framing

        os.flush_page();
    }

    // Comment packet
    {
        os.write_packet_header(3);

        constexpr char vendor[] = "Converted using NME2";

        os << VarInt(sizeof(vendor) + 1, 32);
        
        for (uint8_t i = 0; i < sizeof(vendor) + 1; i++) {
            os << VarInt(vendor[i], 8);
        }

        if (loop_count == 0) {
            os << VarInt(0, 32); // User comment count
        } else {
            os << VarInt(2, 32); // User comment count

            char loop_start_str[21];
            char loop_end_str[19];

            sprintf_s(loop_start_str, sizeof(loop_start_str), "LoopStart=%i", loop_start);
            sprintf_s(loop_end_str, sizeof(loop_end_str), "LoopEnd=%i", loop_end);

            os << VarInt(sizeof(loop_start_str), 32);
            for (uint8_t i = 0; i < sizeof(loop_start_str); i++) {
                os << VarInt(loop_start_str[i], 8);
            }

            os << VarInt(sizeof(loop_end_str), 32);
            for (uint8_t i = 0; i < sizeof(loop_end_str); i++) {
                os << VarInt(loop_end_str[i], 8);
            }
        }

        os << VarInt(1, 1); // Framing

        os.flush_page();
    }

    // Setup packet
    bool* mode_blockflag = nullptr;
    int32_t mode_bits = 0;
    bool prev_blockflag = false;
    {
        os.write_packet_header(5);

        VorbisPacket setup_packet(input, data_offset + setup_packet_offset);

        input->seek(setup_packet.packet_offset());

        BitStream stream(input);

        VarInt codebook_count_less1(0, 8);
        stream >> codebook_count_less1;

        uint32_t codebook_count = codebook_count_less1.val + 1;

        os << codebook_count_less1;

        CodebookLibrary cbl;

        QFile pcbf(":/pcb/packed_codebooks_aoTuV_603.bin");
        pcbf.open(QIODevice::ReadOnly);
        char* pcb = pcbf.readAll().data();
        pcbf.close();

        memcpy_s(cbl.data, CodebookLibrary::offset_offset, pcb, CodebookLibrary::offset_offset);

        for (int64_t i = 0; i < CodebookLibrary::codebook_count; i++)
            cbl.offsets[i] = read_32_le(reinterpret_cast<unsigned char*>(&pcb[CodebookLibrary::offset_offset + i * 4]));
        
        for (uint32_t i = 0; i < codebook_count; i++) {
            VarInt codebook_id(0, 10);

            stream >> codebook_id;

            uint64_t cb_size = cbl.offsets[codebook_id + 1] - cbl.offsets[codebook_id];


        }
    }

    return ogg;
}

bool WWRiffReader::BitStream::get_bit() {
    if (bits_left == 0) {
        if (input->atEnd())
            throw BitStreamError("Out of bits");

        bit_buffer = *input->read(1).data();
        bits_left = 8;
    }

    bits_read++;
    bits_left--;
    
    return ((bit_buffer & (0x80 >> bits_left)) != 0);
}

void WWRiffReader::OggStream::write_packet_header(uint8_t type) {
    *this << VarInt(type, 8);

    constexpr char vorbis[6] = { 'v', 'o', 'r', 'b', 'i', 's' };

    for (uint8_t i = 0; i < 6; i++) {
        *this << VarInt(vorbis[i], 8);
    }
}

void WWRiffReader::OggStream::put_bit(bool bit) {
    if (bit)
        bit_buffer |= 1 << bits_stored;

    bits_stored++;

    if (bits_stored == 8) {
        flush_bits();
    }
}

void WWRiffReader::OggStream::flush_bits() {
    if (bits_stored != 0) {
        if (payload_bytes == n_segment_size * n_max_segments) {
            flush_page(true);

            throw WWRiffFormatError("Ran out of space in an Ogg packet");
        }

        page_buffer[n_header_bytes + n_max_segments + payload_bytes] = bit_buffer;

        payload_bytes++;
        bits_stored = 0;
        bit_buffer = 0;
    }
}

void WWRiffReader::OggStream::flush_page(bool next_continued, bool last) {
    if (payload_bytes != n_segment_size * n_max_segments) {
        flush_bits();
    }

    if (payload_bytes != 0) {
        uint32_t segments = (payload_bytes + n_segment_size) / n_segment_size;
        if (segments == n_max_segments + 1) {
            segments = n_max_segments;
        }

        for (uint32_t i = 0; i < payload_bytes; i++) {
            page_buffer[n_header_bytes + segments + i] = page_buffer[n_header_bytes + n_max_segments + i];
        }

        page_buffer[0] = 'O';
        page_buffer[1] = 'g';
        page_buffer[2] = 'g';
        page_buffer[3] = 'S';
        page_buffer[4] = '\0';
        page_buffer[5] = (continued ? 1 : 0) | (first ? 2 : 0) | (last ? 4 : 0);
        write_32_le(&page_buffer[6], granule);
        write_32_le(&page_buffer[10], 0);
        if (granule == 0xFFFFFFFF) {
            write_32_le(&page_buffer[10], 0xFFFFFFFF);
        }
        write_32_le(&page_buffer[14], 1);
        write_32_le(&page_buffer[18], seqno);
        write_32_le(&page_buffer[22], 0);
        page_buffer[26] = segments;

        for (uint32_t i = 0, bytes_left = payload_bytes; i < segments; i++) {
            if (bytes_left >= n_segment_size) {
                bytes_left -= n_segment_size;
                page_buffer[27 + i] = n_segment_size;
            } else {
                page_buffer[27 + i] = bytes_left;
            }
        }

        write_32_le(&page_buffer[22], checksum(page_buffer, n_header_bytes + segments + payload_bytes));

        output->write(reinterpret_cast<char*>(page_buffer), 27 + segments + payload_bytes);

        seqno++;
        first = false;
        continued = next_continued;
        payload_bytes = 0;
    }
}

uint32_t WWRiffReader::OggStream::checksum(unsigned char* data, uint32_t bytes) {
    uint32_t crc = 0;
    const uint32_t crc_lookup[256] = {
        0x00000000,0x04c11db7,0x09823b6e,0x0d4326d9,0x130476dc,0x17c56b6b,0x1a864db2,0x1e475005,0x2608edb8,0x22c9f00f,0x2f8ad6d6,0x2b4bcb61,0x350c9b64,0x31cd86d3,0x3c8ea00a,0x384fbdbd,
        0x4c11db70,0x48d0c6c7,0x4593e01e,0x4152fda9,0x5f15adac,0x5bd4b01b,0x569796c2,0x52568b75,0x6a1936c8,0x6ed82b7f,0x639b0da6,0x675a1011,0x791d4014,0x7ddc5da3,0x709f7b7a,0x745e66cd,
        0x9823b6e0,0x9ce2ab57,0x91a18d8e,0x95609039,0x8b27c03c,0x8fe6dd8b,0x82a5fb52,0x8664e6e5,0xbe2b5b58,0xbaea46ef,0xb7a96036,0xb3687d81,0xad2f2d84,0xa9ee3033,0xa4ad16ea,0xa06c0b5d,
        0xd4326d90,0xd0f37027,0xddb056fe,0xd9714b49,0xc7361b4c,0xc3f706fb,0xceb42022,0xca753d95,0xf23a8028,0xf6fb9d9f,0xfbb8bb46,0xff79a6f1,0xe13ef6f4,0xe5ffeb43,0xe8bccd9a,0xec7dd02d,
        0x34867077,0x30476dc0,0x3d044b19,0x39c556ae,0x278206ab,0x23431b1c,0x2e003dc5,0x2ac12072,0x128e9dcf,0x164f8078,0x1b0ca6a1,0x1fcdbb16,0x018aeb13,0x054bf6a4,0x0808d07d,0x0cc9cdca,
        0x7897ab07,0x7c56b6b0,0x71159069,0x75d48dde,0x6b93dddb,0x6f52c06c,0x6211e6b5,0x66d0fb02,0x5e9f46bf,0x5a5e5b08,0x571d7dd1,0x53dc6066,0x4d9b3063,0x495a2dd4,0x44190b0d,0x40d816ba,
        0xaca5c697,0xa864db20,0xa527fdf9,0xa1e6e04e,0xbfa1b04b,0xbb60adfc,0xb6238b25,0xb2e29692,0x8aad2b2f,0x8e6c3698,0x832f1041,0x87ee0df6,0x99a95df3,0x9d684044,0x902b669d,0x94ea7b2a,
        0xe0b41de7,0xe4750050,0xe9362689,0xedf73b3e,0xf3b06b3b,0xf771768c,0xfa325055,0xfef34de2,0xc6bcf05f,0xc27dede8,0xcf3ecb31,0xcbffd686,0xd5b88683,0xd1799b34,0xdc3abded,0xd8fba05a,
        0x690ce0ee,0x6dcdfd59,0x608edb80,0x644fc637,0x7a089632,0x7ec98b85,0x738aad5c,0x774bb0eb,0x4f040d56,0x4bc510e1,0x46863638,0x42472b8f,0x5c007b8a,0x58c1663d,0x558240e4,0x51435d53,
        0x251d3b9e,0x21dc2629,0x2c9f00f0,0x285e1d47,0x36194d42,0x32d850f5,0x3f9b762c,0x3b5a6b9b,0x0315d626,0x07d4cb91,0x0a97ed48,0x0e56f0ff,0x1011a0fa,0x14d0bd4d,0x19939b94,0x1d528623,
        0xf12f560e,0xf5ee4bb9,0xf8ad6d60,0xfc6c70d7,0xe22b20d2,0xe6ea3d65,0xeba91bbc,0xef68060b,0xd727bbb6,0xd3e6a601,0xdea580d8,0xda649d6f,0xc423cd6a,0xc0e2d0dd,0xcda1f604,0xc960ebb3,
        0xbd3e8d7e,0xb9ff90c9,0xb4bcb610,0xb07daba7,0xae3afba2,0xaafbe615,0xa7b8c0cc,0xa379dd7b,0x9b3660c6,0x9ff77d71,0x92b45ba8,0x9675461f,0x8832161a,0x8cf30bad,0x81b02d74,0x857130c3,
        0x5d8a9099,0x594b8d2e,0x5408abf7,0x50c9b640,0x4e8ee645,0x4a4ffbf2,0x470cdd2b,0x43cdc09c,0x7b827d21,0x7f436096,0x7200464f,0x76c15bf8,0x68860bfd,0x6c47164a,0x61043093,0x65c52d24,
        0x119b4be9,0x155a565e,0x18197087,0x1cd86d30,0x029f3d35,0x065e2082,0x0b1d065b,0x0fdc1bec,0x3793a651,0x3352bbe6,0x3e119d3f,0x3ad08088,0x2497d08d,0x2056cd3a,0x2d15ebe3,0x29d4f654,
        0xc5a92679,0xc1683bce,0xcc2b1d17,0xc8ea00a0,0xd6ad50a5,0xd26c4d12,0xdf2f6bcb,0xdbee767c,0xe3a1cbc1,0xe760d676,0xea23f0af,0xeee2ed18,0xf0a5bd1d,0xf464a0aa,0xf9278673,0xfde69bc4,
        0x89b8fd09,0x8d79e0be,0x803ac667,0x84fbdbd0,0x9abc8bd5,0x9e7d9662,0x933eb0bb,0x97ffad0c,0xafb010b1,0xab710d06,0xa6322bdf,0xa2f33668,0xbcb4666d,0xb8757bda,0xb5365d03,0xb1f740b4 };

    for (int32_t i = 0; i < bytes; i++)
        crc = (crc << 8) ^ crc_lookup[((crc >> 24) & 0xFF) ^ data[i]];

    return crc;
}

std::vector<QStandardItem*> WWRiffReader::file_contents() {
    std::vector<QStandardItem*> result;

    uint32_t i = 0;
    for (WWRiffFile f : file_table) {
        QStandardItem* item = new QStandardItem(file.baseName() + "_" + QString::number(i++));
        item->setEditable(false);
        item->setIcon(file_icons[NME2::TypeAudio]);
        item->setData(QVariant::fromValue(this), NME2::ReaderRole);
        item->setData(QVariant::fromValue(f), NME2::EmbeddedRole);
        item->setData("WWRiff", NME2::ReaderTypeRole);

        result.push_back(item);
    }

    return result;
}