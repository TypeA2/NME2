#include "bitmanip.h"

uint64_t split_bytes(char* search, uint64_t search_len, char* delimiter, uint64_t delimiter_len, uint64_t start) {
    uint64_t pointer = start;

    while (memcmp(&search[pointer], delimiter, delimiter_len) != 0) {
        if (pointer >= search_len) {
            return -1;
        }
        pointer++;
    }

    return (pointer == 0) ? 0 : pointer - 1;
}

uint16_t read_16_buf(unsigned char b[2]) {
    uint16_t v = 0;
    for (int i = 1; i >= 0; i--) {
        v <<= 8;
        v |= b[i];
    }

    return v;
}

uint16_t read_16_membuf(membuf* buf) {
    uint16_t val = read_16_buf(&buf->data[buf->pos]);
    buf->pos += 2;
    return val;
}

uint32_t read_32_buf(unsigned char b[4]) {
    uint32_t v = 0;
    for (int i = 3; i >= 0; i--) {
        v <<= 8;
        v |= b[i];
    }

    return v;
}

uint32_t read_32_membuf(membuf* buf) {
    uint32_t val = read_32_buf(&buf->data[buf->pos]);
    buf->pos += 4;
    return val;
}

uint_var new_uint_var(uint32_t v, uint64_t bit_size) {
    uint_var bit;
    bit.value = v;
    bit.n_bits = bit_size;
    return bit;
}

ogg_output_stream new_ogg_output_stream(FILE* stream) {
    ogg_output_stream s;
    s.out_stream = stream;
    s.bit_buffer = 0;
    s.bits_stored = 0;
    s.payload_bytes = 0;
    s.granule = 0;
    s.seqno = 0;
    s.first = false;
    s.continued = false;

    return s;
}

void ogg_write(ogg_output_stream* os, uint_var bits) {
    for (unsigned int i = 0; i < bits.n_bits; i++) {
        put_bit(os, (bits.value & (1U << i)) != 0);
    }
}

void put_bit(ogg_output_stream* os, bool bit) {
    if (bit) {
        os->bit_buffer |= 1 << os->bits_stored;
    }

    os->bits_stored += 1;
    if (os->bits_stored == 8) {
        flush_bits(os);
    }
}

void flush_bits(ogg_output_stream* os) {
    if (os->bits_stored != 0) {
        if (os->payload_bytes == SEGMENT_SIZE * MAX_SEGMENTS) {
            perrf("Ran out of space in an Ogg packet: %i %s %i\n", os->bits_stored, os->page_buffer, os->payload_bytes);
            flush_page(os, true, false);
            exit(1);
        }

        os->page_buffer[HEADER_BYTES + MAX_SEGMENTS + os->payload_bytes] = os->bit_buffer;

        os->payload_bytes += 1;

        os->bits_stored = 0;
        os->bit_buffer = 0;
    }
}

void flush_page(ogg_output_stream* os, bool next_continued, bool last) {
    if (os->payload_bytes != SEGMENT_SIZE * MAX_SEGMENTS) {
        flush_bits(os);
    }
    if (os->payload_bytes != 0) {
        unsigned int segments = (os->payload_bytes + SEGMENT_SIZE) / SEGMENT_SIZE;
        if (segments == MAX_SEGMENTS + 1) {
            segments = MAX_SEGMENTS;
        }

        for (unsigned int i = 0; i < os->payload_bytes; i++) {
            os->page_buffer[HEADER_BYTES + segments + i] = os->page_buffer[282 + i];
        }

        os->page_buffer[0] = 'O';
        os->page_buffer[1] = 'g';
        os->page_buffer[2] = 'g';
        os->page_buffer[3] = 'S';
        os->page_buffer[4] = '\0';
        os->page_buffer[5] = (os->continued ? 1 : 0) | (os->first ? 2 : 0) | (last ? 4 : 0);
        write_32(&os->page_buffer[6], os->granule);
        write_32(&os->page_buffer[10], 0);
        if (os->granule == UINT32_C(0xFFFFFFFF)) {
            write_32(&os->page_buffer[10], UINT32_C(0xFFFFFFFF));
        }
        write_32(&os->page_buffer[14], 1);
        write_32(&os->page_buffer[18], os->seqno);
        write_32(&os->page_buffer[22], 0);
        os->page_buffer[26] = segments;

        for (unsigned int i = 0, bytes_left = os->payload_bytes; i < segments; i++) {
            if (bytes_left >= SEGMENT_SIZE) {
                bytes_left -= SEGMENT_SIZE;
                os->page_buffer[27 + i] = SEGMENT_SIZE;
            } else {
                os->page_buffer[27 + i] = bytes_left;
            }
        }

        write_32(&os->page_buffer[22], checksum(os->page_buffer, HEADER_BYTES + segments + os->payload_bytes));

        for (unsigned int i = 0; i < 27 + segments + os->payload_bytes; i++) {
            fputc(os->page_buffer[i], os->out_stream);
        }

        os->seqno += 1;
        os->first = false;
        os->continued = next_continued;
        os->payload_bytes = 0;
    }
}

void write_32(unsigned char b[4], uint32_t v) {
    for (int i = 0; i < 4; i++) {
        b[i] = v & 0xFF;
        v >>= 8;
    }
}

uint32_t checksum(unsigned char* data, int bytes) {
    uint32_t crc_reg = 0;
    uint32_t crc_lookup[256] = {
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
    int i;

    for (i = 0; i < bytes; ++i)
        crc_reg = (crc_reg << 8) ^ crc_lookup[((crc_reg >> 24) & 0xff) ^ data[i]];

    return crc_reg;
}

void ogg_write_vph(ogg_output_stream* os, uint8_t type) {
    ogg_write(os, new_uint_var(type, 8));

    const char vorbis[6] = { 'v', 'o', 'r', 'b', 'i', 's' };

    for (unsigned int i = 0; i < 6; i++) {
        ogg_write(os, new_uint_var(vorbis[i], 8));
    }
}

Packet packet(membuf* data, long offset) {
    data->pos = offset;

    Packet packet;
    packet.offset = offset;
    packet.size = read_16_membuf(data);
    packet.absolute_granule = 0;

    return packet;
}

long packet_offset(Packet packet) {
    return packet.offset + 2;
}

long packet_next_offset(Packet packet) {
    return packet.offset + 2 + packet.size;
}

bit_stream new_bit_stream(membuf* data) {
    bit_stream bs;
    bs.data = data;
    bs.bit_buffer = 0;
    bs.bits_left = 0;
    bs.total_bits_read = 0;
    return bs;
}

void bs_read(bit_stream* bstream, uint_var* uv) {
    uv->value = 0;
    for (unsigned int i = 0; i < uv->n_bits; i++) {
        if (get_bit(bstream)) {
            uv->value |= (1U << i);
        }
    }
}

bool get_bit(bit_stream* bs) {
    if (bs->bits_left == 0) {
        int c = membufgetc(bs->data);
        bs->bit_buffer = c;
        bs->bits_left = 8;
    }
    bs->total_bits_read += 1;
    bs->bits_left -= 1;
    return ((bs->bit_buffer & (0x80 >> bs->bits_left)) != 0);
}

void parse_codebook(bit_stream* bs, int size, ogg_output_stream* os) {
    uint_var dimensions = new_uint_var(0, 4);
    uint_var entries = new_uint_var(0, 14);

    bs_read(bs, &dimensions);
    bs_read(bs, &entries);

    ogg_write(os, new_uint_var(0x564342, 24));
    ogg_write(os, new_uint_var(dimensions.value, 16));
    ogg_write(os, new_uint_var(entries.value, 24));

    uint_var ordered = new_uint_var(0, 1);
    bs_read(bs, &ordered);
    ogg_write(os, ordered);
    uint_var codeword_length_length = new_uint_var(0, 3);
    uint_var sparse = new_uint_var(0, 1);

    bs_read(bs, &codeword_length_length);
    bs_read(bs, &sparse);

    ogg_write(os, sparse);

    for (unsigned int i = 0; i < entries.value; i++) {
        bool present_bool = true;

        if (sparse.value) {
            uint_var present = new_uint_var(0, 1);
            bs_read(bs, &present);
            ogg_write(os, present);

            present_bool = present.value != 0;
        }

        if (present_bool) {
            uint_var codeword_length = new_uint_var(0, codeword_length_length.value);
            bs_read(bs, &codeword_length);
            ogg_write(os, new_uint_var(codeword_length.value, 5));

        }
    }

    uint_var lookup_type = new_uint_var(0, 1);
    bs_read(bs, &lookup_type);
    ogg_write(os, new_uint_var(lookup_type.value, 4));

    if (lookup_type.value == 1) {
        uint_var min = new_uint_var(0, 32);
        uint_var max = new_uint_var(0, 32);
        uint_var value_length = new_uint_var(0, 4);
        uint_var sequence_flag = new_uint_var(0, 1);

        bs_read(bs, &min);
        bs_read(bs, &max);
        bs_read(bs, &value_length);
        bs_read(bs, &sequence_flag);

        ogg_write(os, min);
        ogg_write(os, max);
        ogg_write(os, value_length);
        ogg_write(os, sequence_flag);

        unsigned int quantvals = _book_maptype1_quantvals(entries.value, dimensions.value);

        for (unsigned int i = 0; i < quantvals; i++) {
            uint_var val = new_uint_var(0, value_length.value + 1);
            bs_read(bs, &val);
            ogg_write(os, val);
        }
    }
}

int ilog(unsigned int v) {
    int ret = 0;

    while (v) {
        ret++;
        v >>= 1;
    }

    return ret;
}

unsigned int _book_maptype1_quantvals(unsigned int entries, unsigned int dimensions) {
    int bits = ilog(entries);
    int vals = entries >> ((bits - 1) * (dimensions - 1) / dimensions);

    while (1) {
        unsigned long acc = 1;
        unsigned long acc1 = 1;

        for (unsigned int i = 0; i < dimensions; i++) {
            acc *= vals;
            acc1 *= vals + 1;
        }

        if (acc <= entries && acc1 > entries) {
            return vals;
        } else {
            if (acc > entries) {
                vals--;
            } else {
                vals++;
            }
        }
    }
}

int membufgetc(membuf* buf) {
    buf->pos += 1;
    return (unsigned char)buf->data[buf->pos - 1];
}