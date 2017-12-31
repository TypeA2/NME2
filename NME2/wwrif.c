#include "wwriff.h"

errno_t create_ogg(membuf* data, FILE* out) {
    // Check if the RIFF header is valid
    long riff_size = -1;

    {
        unsigned char riff_header[4];
        unsigned char wave_header[4];

        // The file should start with RIFF
        memcpy_s(riff_header, 4, data->data, 4);
        data->pos += 4;

        // After the RIFF header follows the length of the file, minus the header and the length
        riff_size = read_32_membuf(data) + 8;

        // If the size in the header is larger than the total data size the file is incomplete
        if (riff_size > (long)data->size) {
            perrf("RIFF truncated\n");

            return 1;
        }

        // We require a WAVE header after the riff length value
        memcpy_s(wave_header, 4, &data->data[data->pos], 4);
        data->pos += 4;
        if (memcmp(&wave_header[0], "WAVE", 4) != 0) {
            perrf("Missing WAVE\n");

            return 1;
        }
    }

    // Read and store the offset and size of each chunk type
    long chunk_offset = 12;

    long fmt_offset = -1;
    long fmt_size = -1;

    long cue_offset = -1;
    long cue_size = -1;

    long LIST_offset = -1;
    long LIST_size = -1;

    long smpl_offset = -1;
    long smpl_size = -1;

    long vorb_offset = -1;
    long vorb_size = -1;

    long data_offset = -1;
    long data_size = -1;

    // Loop as long as there's data left
    while (chunk_offset < riff_size) {
        data->pos = chunk_offset;

        // 8 bytes are required to fit the chunk header and length specifier
        if (chunk_offset + 8 > riff_size) {
            perrf("Chunk header %li truncated", chunk_offset);

            return 1;
        }

        // Read the chunk type
        char chunk_type[4];
        memcpy_s(chunk_type, 4, &data->data[data->pos], 4);
        data->pos += 4;

        // Read the chunk length;
        uint32_t chunk_size = read_32_membuf(data);

        if (memcmp(chunk_type, "fmt ", 4) == 0) {
            fmt_offset = chunk_offset + 8;
            fmt_size = chunk_size;
        } else if (memcmp(chunk_type, "cue ", 4) == 0) {
            cue_offset = chunk_offset + 8;
            cue_size = chunk_size;
        } else if (memcmp(chunk_type, "LIST", 4) == 0) {
            LIST_offset = chunk_offset + 8;
            LIST_size = chunk_size;
        } else if (memcmp(chunk_type, "smpl", 4) == 0) {
            smpl_offset = chunk_offset + 8;
            smpl_size = chunk_size;
        } else if (memcmp(chunk_type, "vorb", 4) == 0) {
            vorb_offset = chunk_offset + 8;
            vorb_size = chunk_size;
        } else if (memcmp(chunk_type, "data", 4) == 0) {
            data_offset = chunk_offset + 8;
            data_size = chunk_size;
        }
        chunk_offset = 8 + chunk_offset + chunk_size;
    }

    // Error checking
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint32_t avg_bytes_per_second = 0;
    uint16_t ext_unk = 0;
    uint32_t subtype = 0;
    {
        // There are not enough bytes left to read the entire chunk
        if (chunk_offset > riff_size) {
            perrf("Chunk incomplete\n");

            return 1;
        }

        // The fmt and/or data chunks were not found
        if (fmt_offset == -1 && data_offset == -1) {
            perrf("fmt and data chunks are required\n");

            return 1;
        }

        // If the vorb chunk is missing the size of the fmt chunk should be 0x42 (66)
        if (vorb_offset == -1 && fmt_size != 0x42) {
            perrf("fmt chunk should have a size of 0x42 if vorb chunk is missing\n");

            return 1;
        }

        // If the fmt size makes no sense
        if (vorb_offset != -1 && fmt_size != 0x28 && fmt_size != 0x18 && fmt_size != 0x12) {
            perrf("Invalid fmt size\n");

            return 1;
        }

        // if vorb is missing and fmt_size is 0x42, fmt_offset + 0x18 acts the same as vorb_offset
        if (vorb_offset == -1 && fmt_size == 0x42) {
            vorb_offset = fmt_offset + 0x18;
        }

        // Reads some miscellaneous values
        data->pos = fmt_offset;
        if (read_16_membuf(data) != UINT16_C(0xFFFF)) {
            perrf("Invalid codec id\n");

            return 1;
        }

        channels = read_16_membuf(data);
        sample_rate = read_32_membuf(data);
        avg_bytes_per_second = read_32_membuf(data);

        // End with two null characters
        if (read_16_membuf(data) != 0U) {
            perrf("Invalid block alignment\n");

            return 1;
        }

        // This should return 0
        if (read_16_membuf(data) != 0U) {
            perrf("0bps is required");

            return 1;
        }

        if (fmt_size - 0x12 != read_16_membuf(data)) {
            perrf("Bad extra format length");

            return 1;
        }

        if (fmt_size - 0x12 >= 2) {
            ext_unk = read_16_membuf(data);

            if (fmt_size - 0x12 >= 6) {
                subtype = read_32_membuf(data);
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

            memcpy_s(buf, 16, &data->data[data->pos], 16);

            if (memcmp(buf, buf_check, 16) != 0) {
                perrf("Expected signature in extra fmt");

                return 1;
            }
        }
    }


    // Read cue
    uint32_t cue_count = 0;
    if (cue_offset != -1) {
        data->pos = cue_offset;

        cue_count = read_32_membuf(data);
    }

    // Read smpl
    uint32_t loop_count = 0;
    uint32_t loop_start = 0;
    uint32_t loop_end = 0;
    if (smpl_offset != -1) {
        data->pos = smpl_offset + 0x1C;
        loop_count = read_32_membuf(data);

        if (loop_count != 1) {
            perrf("Expected one loop");

            return 1;
        }

        data->pos = smpl_offset + 0x2C;

        loop_start = read_32_membuf(data);
        loop_end = read_32_membuf(data);
    }

    // Read vorb
    switch (vorb_size) {
        case -1:
        case 0x28:
        case 0x2A:
        case 0x2C:
        case 0x32:
        case 0x34:
            data->pos = vorb_offset;
            break;
        default:
            perrf("Bad vorb size");

            return 1;
    }

    uint32_t sample_count = read_32_membuf(data);

    data->pos = vorb_offset + 0x10;

    uint32_t setup_packet_offset = read_32_membuf(data);
    uint32_t first_audio_packet_offset = read_32_membuf(data);

    switch (vorb_size) {
        case -1:
        case 0x2A:
            data->pos = vorb_offset + 0x24;
            break;
        case 0x32:
        case 0x34:
            data->pos = vorb_offset + 0x2C;
            break;
    }

    uint32_t uid = 0;
    uint8_t blocksize_0_pow = 0;
    uint8_t blocksize_1_pow = 0;
    switch (vorb_size) {
        case -1:
        case 0x2A:
        case 0x32:
        case 0x34:
            uid = read_32_membuf(data);
            blocksize_0_pow = membufgetc(data);
            blocksize_1_pow = membufgetc(data);
            break;
    }

    if (loop_count != 0) {
        if (loop_end == 0) {
            loop_end = sample_count;
        } else {
            loop_end = loop_end + 1;
        }

        if (loop_start >= sample_count || loop_end > sample_count || loop_start > loop_end) {
            perrf("Loops out of range");

            return 1;
        }
    }

    ogg_output_stream os = new_ogg_output_stream(out);

    // ID packet
    {
        ogg_write_vph(&os, 1);

        uint_var version = new_uint_var(0, 32);
        ogg_write(&os, version);

        uint_var ch = new_uint_var(channels, 8);
        ogg_write(&os, ch);

        uint_var srate = new_uint_var(sample_rate, 32);
        ogg_write(&os, srate);

        uint_var bitrate_max = new_uint_var(0, 32);
        ogg_write(&os, bitrate_max);

        uint_var bitrate_nominal = new_uint_var(avg_bytes_per_second * 8, 32);
        ogg_write(&os, bitrate_nominal);

        uint_var bitrate_minimum = new_uint_var(0, 32);
        ogg_write(&os, bitrate_minimum);

        uint_var blocksize_0 = new_uint_var(blocksize_0_pow, 4);
        ogg_write(&os, blocksize_0);

        uint_var blocksize_1 = new_uint_var(blocksize_1_pow, 4);
        ogg_write(&os, blocksize_1);

        uint_var framing = new_uint_var(1, 1);
        ogg_write(&os, framing);

        flush_page(&os, false, false);
    }

    // Comment packet
    {
        ogg_write_vph(&os, 3);

        const char vendor[] = "Converted using NME2";
        uint_var vendor_size = new_uint_var((uint32_t)strlen(vendor), 32);
        ogg_write(&os, vendor_size);

        for (unsigned int i = 0; i < vendor_size.value; i++) {
            uint_var c = new_uint_var(vendor[i], 8);
            ogg_write(&os, c);
        }

        if (loop_count == 0) {
            uint_var user_comment_count = new_uint_var(0, 32);
            ogg_write(&os, user_comment_count);
        } else {
            uint_var user_comment_count = new_uint_var(2, 32);
            ogg_write(&os, user_comment_count);

            char* loop_start_str = malloc(21);
            char* loop_end_str = malloc(19);

            sprintf_s(loop_start_str, 21, "LoopStart=%i", loop_start);
            sprintf_s(loop_end_str, 19, "LoopEnd=%i", loop_end);

            uint_var loop_start_comment_length = new_uint_var((uint32_t)strlen(loop_start_str), 32);
            ogg_write(&os, loop_start_comment_length);

            for (unsigned int i = 0; i < loop_start_comment_length.value; i++) {
                uint_var c = new_uint_var(loop_start_str[i], 8);
                ogg_write(&os, c);
            }
        }

        uint_var framing = new_uint_var(1, 1);
        ogg_write(&os, framing);

        flush_page(&os, false, false);
    }

    bool* mode_blockflag = NULL;
    int mode_bits = 0;
    bool prev_blockflag = false;
    // Setup packet
    {
        ogg_write_vph(&os, 5);

        Packet setup_packet = packet(data, data_offset + setup_packet_offset);

        data->pos = packet_offset(setup_packet);

        if (setup_packet.absolute_granule != 0) {
            perrf("Setup packet granule is not 0");

            return 1;
        }

        bit_stream ss = new_bit_stream(data);

        uint_var codebook_count_less1 = new_uint_var(0, 8);
        bs_read(&ss, &codebook_count_less1);

        unsigned int codebook_count = codebook_count_less1.value + 1;

        ogg_write(&os, codebook_count_less1);

        codebook_library cbl;
        cbl.codebook_count = CODEBOOK_COUNT;
        cbl.codebook_data = malloc(OFFSET_OFFSET);
        cbl.codebook_offsets = malloc(CODEBOOK_COUNT * sizeof(long));

        memcpy_s(&cbl.codebook_data[0], OFFSET_OFFSET, &pcb[0], OFFSET_OFFSET);

        for (long i = 0; i < CODEBOOK_COUNT; i++) {
            cbl.codebook_offsets[i] = read_32_buf((char*)&pcb[OFFSET_OFFSET + i * 4]);
        }

        for (unsigned int i = 0; i < codebook_count; i++) {
            uint_var codebook_id = new_uint_var(0, 10);
            bs_read(&ss, &codebook_id);
            unsigned long cb_size = cbl.codebook_offsets[codebook_id.value + 1] - cbl.codebook_offsets[codebook_id.value];

            //bit_stream_mem bsm = bit_stream_mem(&cbl.codebook_data[cbl.codebook_offsets[codebook_id.value]], cb_size);
            membuf buf;
            buf.data = &cbl.codebook_data[cbl.codebook_offsets[codebook_id.value]];
            buf.size = cb_size;
            buf.pos = 0;

            bit_stream stream = new_bit_stream(&buf);

            parse_codebook(&stream, cb_size, &os);
        }

        uint_var time_count_less1 = new_uint_var(0, 6);
        ogg_write(&os, time_count_less1);
        uint_var dummy_time_value = new_uint_var(0, 16);
        ogg_write(&os, dummy_time_value);
        {
            uint_var floor_count_less1 = new_uint_var(0, 6);
            bs_read(&ss, &floor_count_less1);
            unsigned int floor_count = floor_count_less1.value + 1;
            ogg_write(&os, floor_count_less1);


            for (unsigned int i = 0; i < floor_count; i++) {
                uint_var floor_type = new_uint_var(1, 16);
                ogg_write(&os, floor_type);

                uint_var floor1_partitions = new_uint_var(0, 5);
                bs_read(&ss, &floor1_partitions);
                ogg_write(&os, floor1_partitions);

                unsigned int* floor1_partition_class_list = malloc(floor1_partitions.value * sizeof(unsigned int));

                unsigned int maximum_class = 0;
                for (unsigned int j = 0; j < floor1_partitions.value; j++) {
                    uint_var floor1_partition_class = new_uint_var(0, 4);
                    bs_read(&ss, &floor1_partition_class);
                    ogg_write(&os, floor1_partition_class);

                    floor1_partition_class_list[j] = floor1_partition_class.value;

                    if (floor1_partition_class.value > maximum_class) {
                        maximum_class = floor1_partition_class.value;
                    }
                }

                unsigned int* floor1_class_dimensions_list = malloc((maximum_class + 1) * sizeof(unsigned int));

                for (unsigned int j = 0; j <= maximum_class; j++) {
                    uint_var class_dimensions_less_1 = new_uint_var(0, 3);
                    bs_read(&ss, &class_dimensions_less_1);
                    ogg_write(&os, class_dimensions_less_1);

                    floor1_class_dimensions_list[j] = class_dimensions_less_1.value + 1;

                    uint_var class_subclasses = new_uint_var(0, 2);
                    bs_read(&ss, &class_subclasses);
                    ogg_write(&os, class_subclasses);

                    if (class_subclasses.value != 0) {
                        uint_var masterbook = new_uint_var(0, 8);
                        bs_read(&ss, &masterbook);
                        ogg_write(&os, masterbook);

                        if (masterbook.value >= codebook_count) {
                            perrf("Invalid floor1 masterbook\n");

                            return 1;
                        }
                    }

                    for (unsigned int k = 0; k < (1U << class_subclasses.value); k++) {
                        uint_var subclass_book_plus1 = new_uint_var(0, 8);
                        bs_read(&ss, &subclass_book_plus1);
                        ogg_write(&os, subclass_book_plus1);

                        int subclass_book = ((int)subclass_book_plus1.value) - 1;

                        if (subclass_book >= 0 && (unsigned int)subclass_book >= codebook_count) {
                            perrf("Invalid floor1 subclass book\n");

                            return 1;
                        }
                    }
                }

                uint_var floor1_multiplier_less1 = new_uint_var(0, 2);
                bs_read(&ss, &floor1_multiplier_less1);
                ogg_write(&os, floor1_multiplier_less1);

                uint_var rangebits = new_uint_var(0, 4);
                bs_read(&ss, &rangebits);
                ogg_write(&os, rangebits);

                for (unsigned int j = 0; j < floor1_partitions.value; j++) {
                    unsigned int current_class_number = floor1_partition_class_list[j];

                    for (unsigned int k = 0; k < floor1_class_dimensions_list[current_class_number]; k++) {
                        uint_var X = new_uint_var(0, rangebits.value);
                        bs_read(&ss, &X);
                        ogg_write(&os, X);
                    }
                }

                free(floor1_class_dimensions_list);
                free(floor1_partition_class_list);
            }

            // Residue count
            uint_var residue_count_less1 = new_uint_var(0, 6);
            bs_read(&ss, &residue_count_less1);
            unsigned int residue_count = residue_count_less1.value + 1;
            ogg_write(&os, residue_count_less1);

            // Rebuild residues
            for (unsigned int i = 0; i < residue_count; i++) {
                uint_var residue_type = new_uint_var(0, 2);
                bs_read(&ss, &residue_type);
                ogg_write(&os, new_uint_var(residue_type.value, 16));

                if (residue_type.value > 2) {
                    perrf("Invalid residue type");

                    return 1;
                }

                uint_var residue_begin = new_uint_var(0, 24);
                uint_var residue_end = new_uint_var(0, 24);
                uint_var residue_partition_size_less1 = new_uint_var(0, 24);
                uint_var residue_classifications_less1 = new_uint_var(0, 6);
                uint_var residue_classbook = new_uint_var(0, 8);

                bs_read(&ss, &residue_begin);
                bs_read(&ss, &residue_end);
                bs_read(&ss, &residue_partition_size_less1);
                bs_read(&ss, &residue_classifications_less1);
                bs_read(&ss, &residue_classbook);

                unsigned int residue_classifications = residue_classifications_less1.value + 1;

                ogg_write(&os, residue_begin);
                ogg_write(&os, residue_end);
                ogg_write(&os, residue_partition_size_less1);
                ogg_write(&os, residue_classifications_less1);
                ogg_write(&os, residue_classbook);

                if (residue_classbook.value >= codebook_count) {
                    perrf("Invalid residue classbook\n");

                    return 1;
                }

                unsigned int* residue_cascade = malloc(residue_classifications * sizeof(unsigned int));

                for (unsigned int j = 0; j < residue_classifications; j++) {
                    uint_var high_bits = new_uint_var(0, 5);
                    uint_var low_bits = new_uint_var(0, 3);

                    bs_read(&ss, &low_bits);
                    ogg_write(&os, low_bits);

                    uint_var bitflag = new_uint_var(0, 1);
                    bs_read(&ss, &bitflag);
                    ogg_write(&os, bitflag);

                    if (bitflag.value) {
                        bs_read(&ss, &high_bits);
                        ogg_write(&os, high_bits);
                    }

                    residue_cascade[j] = high_bits.value * 8 + low_bits.value;
                }

                for (unsigned int j = 0; j < residue_classifications; j++) {
                    for (unsigned int k = 0; k < 8; k++) {
                        if (residue_cascade[j] & (1 << k)) {
                            uint_var residue_book = new_uint_var(0, 8);
                            bs_read(&ss, &residue_book);
                            ogg_write(&os, residue_book);

                            if (residue_book.value >= codebook_count) {
                                perrf("Invalid residue book\n");

                                return 1;
                            }
                        }
                    }
                }

                free(residue_cascade);
            }

            // Mapping count
            uint_var mapping_count_less1 = new_uint_var(0, 6);
            bs_read(&ss, &mapping_count_less1);
            unsigned int mapping_count = mapping_count_less1.value + 1;
            ogg_write(&os, mapping_count_less1);

            for (unsigned int i = 0; i < mapping_count; i++) {
                uint_var mapping_type = new_uint_var(0, 16);
                ogg_write(&os, mapping_type);

                uint_var submaps_flag = new_uint_var(0, 1);
                bs_read(&ss, &submaps_flag);
                ogg_write(&os, submaps_flag);

                unsigned int submaps = 1;
                if (submaps_flag.value) {
                    uint_var submaps_less1 = new_uint_var(0, 4);

                    bs_read(&ss, &submaps_less1);
                    submaps = submaps_less1.value + 1;
                    ogg_write(&os, submaps_less1);
                }

                uint_var square_polar_flag = new_uint_var(0, 1);
                bs_read(&ss, &square_polar_flag);
                ogg_write(&os, square_polar_flag);

                if (square_polar_flag.value) {
                    uint_var coupling_steps_less1 = new_uint_var(0, 8);
                    bs_read(&ss, &coupling_steps_less1);
                    unsigned int coupling_steps = coupling_steps_less1.value + 1;
                    ogg_write(&os, coupling_steps_less1);

                    for (unsigned int j = 0; j < coupling_steps; j++) {
                        uint_var magnitude = new_uint_var(0, ilog(channels - 1));
                        uint_var angle = new_uint_var(0, ilog(channels - 1));

                        bs_read(&ss, &magnitude);
                        bs_read(&ss, &angle);

                        ogg_write(&os, magnitude);
                        ogg_write(&os, angle);


                        if (angle.value == magnitude.value || magnitude.value >= channels || angle.value >= channels) {
                            perrf("Invalid coupling\n");

                            return 1;
                        }
                    }
                }

                uint_var mapping_reserved = new_uint_var(0, 2);
                bs_read(&ss, &mapping_reserved);
                ogg_write(&os, mapping_reserved);
                if (mapping_reserved.value != 0) {
                    perrf("Mapping reserved field nonzero\n");

                    return 1;
                }

                if (submaps > 1) {
                    for (unsigned int j = 0; j < channels; j++) {
                        uint_var mapping_mux = new_uint_var(0, 4);
                        bs_read(&ss, &mapping_mux);
                        ogg_write(&os, mapping_mux);

                        if (mapping_mux.value >= submaps) {
                            perrf("mapping_mux >= submaps\n");

                            return 1;
                        }
                    }
                }

                for (unsigned int j = 0; j < submaps; j++) {
                    uint_var time_config = new_uint_var(0, 8);
                    bs_read(&ss, &time_config);
                    ogg_write(&os, time_config);

                    uint_var floor_number = new_uint_var(0, 8);
                    bs_read(&ss, &floor_number);
                    ogg_write(&os, floor_number);
                    if (floor_number.value >= floor_count) {
                        perrf("Invalid floor mapping\n");

                        return 1;
                    }

                    uint_var residue_number = new_uint_var(0, 8);
                    bs_read(&ss, &residue_number);
                    ogg_write(&os, residue_number);
                    if (residue_number.value >= residue_count) {
                        perrf("Invalid residue mapping\n");

                        return 1;
                    }
                }
            }

            // Mode count
            uint_var mode_count_less1 = new_uint_var(0, 6);
            bs_read(&ss, &mode_count_less1);
            unsigned int mode_count = mode_count_less1.value + 1;
            ogg_write(&os, mode_count_less1);


            mode_blockflag = malloc(mode_count * sizeof(bool));
            mode_bits = ilog(mode_count - 1);

            for (unsigned int i = 0; i < mode_count; i++) {
                uint_var block_flag = new_uint_var(0, 1);
                bs_read(&ss, &block_flag);
                ogg_write(&os, block_flag);

                mode_blockflag[i] = (block_flag.value != 0);

                uint_var windowtype = new_uint_var(0, 16);
                uint_var transformtype = new_uint_var(0, 16);
                ogg_write(&os, windowtype);
                ogg_write(&os, transformtype);

                uint_var mapping = new_uint_var(0, 8);
                bs_read(&ss, &mapping);
                ogg_write(&os, mapping);
                if (mapping.value >= mapping_count) {
                    perrf("Invalid mode mapping\n");

                    return 1;
                }
            }

            uint_var framing = new_uint_var(1, 1);
            ogg_write(&os, framing);
        }
        flush_page(&os, false, false);

        if ((ss.total_bits_read + 7) / 8 != setup_packet.size) {
            perrf("Didn't fully read setup packet\n");

            return 1;
        }

        if (packet_next_offset(setup_packet) != data_offset + (long)first_audio_packet_offset) {
            perrf("First audio packet doesn't follow setup packet\n");

            return 1;
        }
    }

    // Audio pages
    {
        long offset = data_offset + first_audio_packet_offset;

        while (offset < data_offset + data_size) {
            Packet audio_packet = packet(data, offset);
            long packet_header_size = 2;
            uint32_t size = audio_packet.size;
            long packet_payload_offset = packet_offset(audio_packet);
            uint32_t granule = audio_packet.absolute_granule;
            long next_offset = packet_next_offset(audio_packet);

            if (offset + packet_header_size > data_offset + data_size) {
                perrf("Page header truncated\n");

                return 1;
            }

            offset = packet_payload_offset;

            data->pos = offset;

            if (granule == UINT32_C(0xFFFFFFFF)) {
                os.granule = 1;
            } else {
                os.granule = granule;
            }

            // First byte
            if (!mode_blockflag) {
                perrf("Didn't load mode_blockflag\n");

                return 1;
            }

            uint_var packet_type = new_uint_var(0, 1);
            ogg_write(&os, packet_type);

            uint_var* mode_number_p = malloc(sizeof(uint_var));
            uint_var* remainder_p = malloc(sizeof(uint_var));

            {
                bit_stream ss = new_bit_stream(data);
                *mode_number_p = new_uint_var(0, mode_bits);

                bs_read(&ss, mode_number_p);
                ogg_write(&os, *mode_number_p);

                *remainder_p = new_uint_var(0, 8 - mode_bits);
                bs_read(&ss, remainder_p);
            }

            if (mode_blockflag[mode_number_p->value]) {
                data->pos = next_offset;
                bool next_blockflag = false;
                if (next_offset + packet_header_size <= data_offset + data_size) {
                    Packet audio_packet = packet(data, next_offset);
                    uint32_t next_packet_size = audio_packet.size;

                    if (next_packet_size > 0) {
                        data->pos = packet_offset(audio_packet);

                        bit_stream ss = new_bit_stream(data);
                        uint_var next_mode_number = new_uint_var(0, mode_bits);

                        bs_read(&ss, &next_mode_number);

                        next_blockflag = mode_blockflag[next_mode_number.value];
                    }
                }

                uint_var prev_window_type = new_uint_var(prev_blockflag, 1);
                ogg_write(&os, prev_window_type);

                uint_var next_window_type = new_uint_var(next_blockflag, 1);
                ogg_write(&os, next_window_type);
                data->pos = offset + 1;
            }

            prev_blockflag = mode_blockflag[mode_number_p->value];
            free(mode_number_p);
            ogg_write(&os, *remainder_p);
            free(remainder_p);

            for (unsigned int i = 1; i < size; i++) {
                int v = membufgetc(data);
                if (v < 0) {
                    perrf("File truncated: %i %i\n", v, i);

                    return 1;
                }

                uint_var c = new_uint_var(v, 8);
                ogg_write(&os, c);
            }

            offset = next_offset;
            flush_page(&os, (offset == data_offset + data_size), false);
        }

        if (offset > data_offset + data_size) {
            perrf("Page truncated\n");

            return 1;
        }
    }
    free(mode_blockflag);

    return 0;
}