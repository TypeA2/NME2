#include "CripackReader.h"

#include "NME2.h"

CripackReader::CripackReader(QFileInfo file, std::map<uint32_t, QIcon>& icons) : 
    infile(file.canonicalFilePath().toStdString().c_str(), std::ios::binary | std::ios::in),
    file_icons(icons) {

    {
        char cpk_header[4];
        infile.read(cpk_header, 4);

        if (memcmp(cpk_header, "CPK ", 4) != 0) {
            throw FormatError("Invalid CPK header");
        }
    }

    read_utf();

    cpk_packet = utf_packet;

    if (infile.gcount() != utf_size) {
        throw FormatError("Did not read all bytes");
    }

    EmbeddedFile cpak_entry;
    cpak_entry.file_name = "CPK_HDR";
    cpak_entry.file_offset_pos = static_cast<uint64_t>(infile.tellg()) + 0x10;
    cpak_entry.file_size = utf_size;
    cpak_entry.file_type = "CPK";

    file_table.push_back(cpak_entry);

    utf = new UTF(reinterpret_cast<unsigned char*>(utf_packet));

    for (uint32_t i = 0; i < utf->columns.size(); i++) {
        cpkdata.emplace(utf->columns[i].name, utf->rows[0].rows[i].val);
    }
    
    TocOffset = get_column_data(utf, 0, "TocOffset").toULongLong();
    uint64_t TocOffsetPos = get_column_position(utf, 0, "TocOffset");

    EtocOffset = get_column_data(utf, 0, "EtocOffset").toULongLong();
    uint64_t EtocOffsetPos = get_column_position(utf, 0, "EtocOffset");

    ItocOffset = get_column_data(utf, 0, "ItocOffset").toULongLong();
    uint64_t ItocOffsetPos = get_column_position(utf, 0, "ItocOffset");

    GtocOffset = get_column_data(utf, 0, "GtocOffset").toULongLong();
    uint64_t GtocOffsetPos = get_column_position(utf, 0, "GtocOffset");

    get_column_data(utf, 0, "ContentOffset").toULongLong();
    uint64_t ContentOffsetPos = get_column_position(utf, 0, "ContentOffset");

    file_table.push_back(EmbeddedFile("CONTENT_OFFSET", ContentOffset, ItocOffset, "CPK", "HDR"));

    uint32_t n_files = get_column_data(utf, 0, "Files").toUInt();
    alignment = get_column_data(utf, 0, "Align").toUInt();

    if (TocOffset != 0) {
        file_table.push_back(EmbeddedFile("TOC_HDR", TocOffset, TocOffsetPos, "CPK", "HDR"));

        read_toc();
    }

    if (EtocOffset != 0) {
        file_table.push_back(EmbeddedFile("ETOC_HDR", EtocOffset, EtocOffsetPos, "CPK", "HDR"));

        read_etoc();
    }

    if (ItocOffset != 0) {
        file_table.push_back(EmbeddedFile("ITOC_HDR", ItocOffset, ItocOffsetPos, "CPK", "HDR"));

        read_itoc();
    }

    if (GtocOffset != 0) {
        file_table.push_back(EmbeddedFile("GTOC_HDR", GtocOffset, GtocOffsetPos, "CPK", "HDR"));

        read_gtoc();
    }
}

std::vector<QStandardItem*> CripackReader::file_contents() {
    std::vector<std::string> dirs;

    for (EmbeddedFile f : file_table) {
        if (f.file_type != "FILE" || f.dir_name.length() == 0) {
            continue;
        }

        if (std::find(dirs.begin(), dirs.end(), f.dir_name) == dirs.end()) {
            dirs.push_back(f.dir_name);
        }
    }

    return merge_dirs(dirs);
}

std::vector<QStandardItem*> CripackReader::merge_dirs(std::vector<std::string> dirs) {
    std::vector<QStandardItem*> result;
    std::vector<QStandardItem*> file_hold;
    std::map<std::string, QStandardItem*> dir_map;

    QIcon dir_icon = QFileIconProvider().icon(QFileIconProvider::Folder);

    for (EmbeddedFile f : file_table) {
        if (f.file_type != "FILE") {
            continue;
        }

        if (f.dir_name.length() == 0) {
            QStandardItem* file = new QStandardItem(f.file_name.c_str());
            file->setEditable(false);
            file->setIcon(file_icons[NME2::fname_to_icon(f.file_name)]);
            file->setData(QVariant::fromValue(this), NME2::ReaderRole);
            file->setData("Cripack", NME2::ReaderTypeRole);
            file->setData(QVariant::fromValue(f), NME2::EmbeddedRole);

            if (NME2::ends_with(f.file_name, ".dat") || NME2::ends_with(f.file_name, ".dtt")) {
                std::cout << f.file_offset << f.file_name<< std::endl;
                char* data = new char[f.file_size];

                infile.seekg(f.file_offset);
                infile.read(data, f.file_size);

                if (f.file_size != f.extract_size) {
                    char* tmp = decompress_crilayla(data, f.file_size);

                    delete data;

                    data = tmp;
                }

                DATReader* reader = new DATReader(data, file_icons);
                file->appendRows(NME2::vector_to_qlist(reader->file_contents()));
            }

            file_hold.push_back(file);
        } else {
            std::string base_dir = f.dir_name.substr(0, f.dir_name.find('/'));

            if (dir_map.count(base_dir) == 0) {
                QStandardItem* dir = new QStandardItem(base_dir.c_str());
                dir->setEditable(false);
                dir->setIcon(dir_icon);
                dir->setData(QVariant::fromValue(this), NME2::ReaderRole);
                dir->setData("Cripack", NME2::ReaderTypeRole);

                dir->appendRows(NME2::vector_to_qlist<QStandardItem*>(match_dirs(base_dir)));


                dir_map.emplace(base_dir, dir);
            }
        }
    }

    for (std::pair<std::string, QStandardItem*> dir : dir_map) {
        result.push_back(dir.second);
    }

    result.insert(result.end(), file_hold.begin(), file_hold.end());

    return result;
}

std::vector<QStandardItem*> CripackReader::match_dirs(std::string dir) {
    std::vector<QStandardItem*> result;
    std::vector<QStandardItem*> file_hold;
    std::map<std::string, QStandardItem*> dirs;

    QIcon dir_icon = QFileIconProvider().icon(QFileIconProvider::Folder);
    bool dirs_present = false;

    for (EmbeddedFile f : file_table) {
        if (f.file_type != "FILE") {
            continue;
        }

        if (f.dir_name == dir) {
            QStandardItem* file = new QStandardItem(f.file_name.c_str());
            file->setEditable(false);
            file->setIcon(file_icons[NME2::fname_to_icon(f.file_name)]);
            file->setData(QVariant::fromValue(this), NME2::ReaderRole);
            file->setData("Cripack", NME2::ReaderTypeRole);
            file->setData(QVariant::fromValue(f), NME2::EmbeddedRole);

            if (NME2::ends_with(f.file_name, ".dat") || NME2::ends_with(f.file_name, ".dtt")) {
                char* data = new char[f.file_size];

                infile.seekg(f.file_offset);
                infile.read(data, f.file_size);

                if (f.file_size != f.extract_size) {
                    char* tmp = decompress_crilayla(data, f.file_size);

                    delete data;

                    data = tmp;
                }

                DATReader* reader = new DATReader(data, file_icons);
                file->appendRows(NME2::vector_to_qlist(reader->file_contents()));
            }

            file_hold.push_back(file);
        } else if (f.dir_name.compare(0, dir.length(), dir) == 0) {
            dirs_present = true;

            std::string dirname = f.dir_name.substr(f.dir_name.find('/') + 1);

            if (dirs.count(dirname) == 0) {
                QStandardItem* subdir = new QStandardItem(dirname.c_str());
                subdir->setIcon(dir_icon);
                subdir->setEditable(false);
                subdir->setData(QVariant::fromValue(this), NME2::ReaderRole);
                subdir->setData("Cripack", NME2::ReaderTypeRole);

                subdir->appendRows(NME2::vector_to_qlist(match_dirs(dir + "/" + dirname)));

                dirs.emplace(dirname, subdir);
            }
        }
    }

    if (dirs_present) {
        for (std::pair<std::string, QStandardItem*> dir : dirs) {
            result.push_back(dir.second);
        }
    }

    result.insert(result.end(), file_hold.begin(), file_hold.end());

    return result;
}

void CripackReader::read_gtoc() {
    infile.seekg(GtocOffset, std::ios::beg);

    {
        char gtoc_hdr[4];
        infile.read(gtoc_hdr, 4);

        if (memcmp(gtoc_hdr, "GTOC", 4) != 0) {
            throw FormatError("Invalid GTOC header");
        }
    }

    read_utf();

    gtoc_packet = utf_packet;

    ptrdiff_t gtoc_file_pos = std::find_if(file_table.begin(), file_table.end(), [](const EmbeddedFile& f) { return f.file_name == "GTOC_HDR"; }) - file_table.begin();
    file_table[gtoc_file_pos].file_size = utf_size;
}

void CripackReader::read_itoc() {
    infile.seekg(ItocOffset, std::ios::beg);

    {
        char itoc_hdr[4];
        infile.read(itoc_hdr, 4);

        if (memcmp(itoc_hdr, "ITOC", 4) != 0) {
            throw FormatError("Invalid ETOC header");
        }
    }

    read_utf();

    itoc_packet = utf_packet;

    ptrdiff_t itoc_file_pos = std::find_if(file_table.begin(), file_table.end(), [](const EmbeddedFile& f) { return f.file_name == "ITOC_HDR"; }) - file_table.begin();
    file_table[itoc_file_pos].file_size = utf_size;

    files = new UTF(reinterpret_cast<unsigned char*>(itoc_packet));

    char* data_l = const_cast<char*>(get_column_data(files, 0, "DataL").toByteArray().constData());
    uint64_t data_l_pos = get_column_position(files, 0, "DataL");

    char* data_h = const_cast<char*>(get_column_data(files, 0, "DataH").toByteArray().constData());
    uint64_t data_h_pos = get_column_position(files, 0, "DataH");

    UTF* utf_data_l;
    UTF* utf_data_h;
    std::map<uint32_t, uint32_t> size_table;
    std::map<uint32_t, uint32_t> c_size_table;
    std::map<uint32_t, uint64_t> size_pos_table;
    std::map<uint32_t, uint64_t> c_size_pos_table;

    std::vector<uint32_t> ids;

    uint16_t id, size1;
    uint32_t size2;
    uint64_t pos;
    uint8_t type;

    if (data_l != nullptr) {
        utf_data_l = new UTF(reinterpret_cast<unsigned char*>(data_l));

        for (uint32_t i = 0; i < utf_data_l->n_rows; i++) {
            id = get_column_data(utf_data_l, i, "ID").toUInt();

            size1 = get_column_data(utf_data_l, i, "FileSize").toUInt();
            size_table.emplace(id, size1);

            pos = get_column_position(utf_data_l, i, "FileSize");
            size_pos_table.emplace(id, pos + data_l_pos);

            if (column_exists(utf_data_l, i, "ExtractSize")) {
                size1 = get_column_data(utf_data_l, i, "ExtractSize").toUInt();
                c_size_table.emplace(id, size1);

                pos = get_column_position(utf_data_l, i, "ExtractSize");
                c_size_pos_table.emplace(id, pos + data_l_pos);
            }

            ids.push_back(id);
        }
    }

    if (data_h != nullptr) {
        utf_data_h = new UTF(reinterpret_cast<unsigned char*>(data_h));

        for (uint32_t i = 0; i < utf_data_h->n_rows; i++) {
            id = get_column_data(utf_data_h, i, "ID").toUInt();

            size2 = get_column_data(utf_data_h, i, "FileSize").toUInt();
            size_table.emplace(id, size2);

            pos = get_column_position(utf_data_h, i, "FileSize");
            size_pos_table.emplace(id, pos + data_l_pos);

            if (column_exists(utf_data_h, i, "ExtractSize")) {
                size2 = get_column_data(utf_data_h, i, "ExtractSize").toUInt();
                c_size_table.emplace(id, size2);

                pos = get_column_position(utf_data_h, i, "ExtractSize");
                c_size_pos_table.emplace(id, pos + data_l_pos);
            }

            ids.push_back(id);
        }
    }

    uint32_t v1 = 0;
    uint32_t v2 = 0;
    uint64_t base_offset = ContentOffset;

    std::sort(ids.begin(), ids.end());

    for (uint32_t i = 0; i < ids.size(); i++) {
        uint32_t id = ids[i];

        EmbeddedFile f;

        if (size_table.find(id) != size_table.end()) {
            v1 = size_table[id];
        }

        if (c_size_table.find(id) != c_size_table.end()) {
            v2 = c_size_table[id];
        }

        f.toc_name = "ITOC";
        f.file_name = std::to_string(id) + ".bin";

        f.file_size = v1;
        f.file_size_pos = size_pos_table[id];

        if (c_size_table.size() > 0 && c_size_table.find(id) != c_size_table.end()) {
            f.extract_size = v2;
            f.extract_size_pos = c_size_pos_table[id];
        }

        f.file_type = "FILE";

        f.file_offset = base_offset;
        f.id = id;
        
        file_table.push_back(f);

        if ((v1 % alignment) > 0) {
            base_offset += v1 + (alignment - (v1 % alignment));
        } else {
            base_offset += v1;
        }
    }

    delete files;
    delete utf_data_l;
    delete utf_data_h;
}

void CripackReader::read_etoc() {
    infile.seekg(EtocOffset, std::ios::beg);

    {
        char etoc_hdr[4];
        infile.read(etoc_hdr, 4);

        if (memcmp(etoc_hdr, "ETOC", 4) != 0) {
            throw FormatError("Invalid ETOC header");
        }
    }

    read_utf();

    etoc_packet = utf_packet;

    ptrdiff_t etoc_file_pos = std::find_if(file_table.begin(), file_table.end(), [](const EmbeddedFile& f) { return f.file_name == "ETOC_HDR"; }) - file_table.begin();
    file_table[etoc_file_pos].file_size = utf_size;

    files = new UTF(reinterpret_cast<unsigned char*>(etoc_packet));

    std::vector<EmbeddedFile> file_entries;

    std::function<bool(const EmbeddedFile& f)> find_file = [](const EmbeddedFile& f) { return f.file_type == "FILE"; };

    std::for_each(file_table.begin(), file_table.end(), [&](const EmbeddedFile& f) {
        if (f.file_type == "FILE") {
            file_entries.push_back(f);
        }
    });

    for (uint32_t i = 0; i < file_entries.size(); i++) {
        file_table[i].local_dir = get_column_data(files, i, "LocalDir").toString().toStdString();
        file_table[i].update_date_time = get_column_data(files, i, "UpdateDateTime").toULongLong();
    }

    delete files;
}

void CripackReader::read_toc() {
    uint64_t tTocOffset = TocOffset;
    uint64_t add_offset = 0;

    if (tTocOffset > 0x800U) {
        tTocOffset = 0x800U;
    }

    if (ContentOffset <= 0) {
        add_offset = tTocOffset;
    } else if (TocOffset <= 0 || ContentOffset < tTocOffset) {
        add_offset = ContentOffset;
    } else {
        add_offset = tTocOffset;
    }

    infile.seekg(TocOffset, std::ios::beg);

    {
        char toc_hdr[4];
        infile.read(toc_hdr, 4);

        if (memcmp(toc_hdr, "TOC ", 4) != 0) {
            throw FormatError("Invalid TOC header");
        }
    }

    read_utf();

    toc_packet = utf_packet;

    ptrdiff_t toc_file_pos = std::find_if(file_table.begin(), file_table.end(), [](const EmbeddedFile& f) { return f.file_name == "TOC_HDR"; }) - file_table.begin();
    file_table[toc_file_pos].file_size = utf_size;

    files = new UTF(reinterpret_cast<unsigned char*>(toc_packet));

    for (uint16_t i = 0; i < files->n_rows; i++) {
        EmbeddedFile f;

        f.toc_name = "TOC";
        
        f.dir_name = get_column_data(files, i, "DirName").toString().toStdString();
        f.file_name = get_column_data(files, i, "FileName").toString().toStdString();

        f.file_size = get_column_data(files, i, "FileSize").toULongLong();
        f.file_size_pos = get_column_position(files, i, "FileSize");
        
        f.extract_size = get_column_data(files, i, "ExtractSize").toULongLong();
        f.extract_size_pos = get_column_position(files, i, "ExtractSize");
        
        f.file_offset = get_column_data(files, i, "FileOffset").toULongLong();
        f.file_offset_pos = get_column_position(files, i, "FileOffset");

        f.file_type = "FILE";

        f.file_offset = add_offset;

        f.id = get_column_data(files, i, "ID").toUInt();

        f.user_string = get_column_data(files, i, "UserString").toString().toStdString();

        file_table.push_back(f);
    }

    delete files;
}

bool CripackReader::column_exists(UTF* utf_src, uint32_t row, std::string name) {
    for (uint16_t i = 0; i < utf_src->n_columns; i++) {
        if (utf_src->columns[i].name == name) {
            return true;
        }
    }

    return false;
}

QVariant CripackReader::get_column_data(UTF* utf_src, uint32_t row, std::string name) {
    QVariant result;

    for (uint16_t i = 0; i < utf_src->n_columns; i++) {
        if (utf_src->columns[i].name == name) {
            result = utf_src->rows[row].rows[i].val;
        }
    }

    return result;
}

uint64_t CripackReader::get_column_position(UTF* utf_src, uint32_t row, std::string name) {
    uint64_t result = 0;

    for (uint16_t i = 0; i < utf_src->n_columns; i++) {
        if (utf_src->columns[i].name == name) {
            result = utf_src->rows[row].rows[i].position;
            break;
        }
    }

    return result;
}

char* CripackReader::decompress_crilayla(char* input, uint64_t input_size) {
    uint64_t pos = 8;

    if (memcmp(input, "CRILAYLA", 8) != 0) {
        std::cout << "CRILAYLA: " << input[0] << input[1] << input[2] << input[3] << input[4] << input[5] << input[6] << input[7] << std::endl;
        throw FormatError("Invalid CRILAYLA signature");
    }
    
    uint32_t uncompressed_size = read_32_le(reinterpret_cast<unsigned char*>(&input[8]));

    uint32_t uncompressed_hdr_offset = read_32_le(reinterpret_cast<unsigned char*>(&input[12]));

    char* result = new char[uncompressed_size + 0x100];

    memcpy_s(result, uncompressed_size + 0x100, &input[uncompressed_hdr_offset + 0x10], input_size);

    uint64_t input_end = input_size - 0x101;
    uint64_t input_offset = input_end;
    uint64_t output_end = 0xff + uncompressed_size;
    uint8_t bit_pool = 0;
    uint64_t bits_left = 0;
    uint64_t bytes_output = 0;
    uint32_t vle_lens[4] = { 2, 3, 5, 8 };

    while (bytes_output < uncompressed_size) {
        if (get_bits(input, &input_offset, &bit_pool, &bits_left, 1) > 0) {
            uint64_t backreference_offset = output_end - bytes_output + get_bits(input, &input_offset, &bit_pool, &bits_left, 13) + 3;
            uint64_t backreference_length = 3;
            uint32_t vle_level;

            for (vle_level = 0; vle_level < 4; vle_level++) {
                uint16_t this_level = get_bits(input, &input_offset, &bit_pool, &bits_left, vle_lens[vle_level]);
                backreference_length += this_level;

                if (this_level != ((1 << vle_lens[vle_level]) - 1)) {
                    break;
                }
            }

            if (vle_level == 4) {
                uint16_t this_level;

                do {
                    this_level = get_bits(input, &input_offset, &bit_pool, &bits_left, 8);
                    backreference_length += this_level;
                } while (this_level == 0xff);
            }

            for (uint64_t i = 0; i < backreference_length; i++) {
                result[output_end - bytes_output] = result[backreference_offset--];
                bytes_output++;
            }
        } else {
            result[output_end - bytes_output] = static_cast<int8_t>(get_bits(input, &input_offset, &bit_pool, &bits_left, 8));
            bytes_output++;
        }
    } 

    return result;
}

CripackReader::UTF::UTF(unsigned char* utf_packet) : utf_packet(utf_packet), pos(0), mem(true) {
    if (memcmp(utf_packet, "@UTF", 4) != 0) {
        throw FormatError("Invalid @UTF header");
    }

    pos += 4;

    table_size = read_32_be(&utf_packet[pos]);
    pos += 4;

    rows_offset = read_32_be(&utf_packet[pos]);
    pos += 4;

    strings_offset = read_32_be(&utf_packet[pos]);
    pos += 4;

    data_offset = read_32_be(&utf_packet[pos]);
    pos += 4;

    rows_offset += 8;
    strings_offset += 8;
    data_offset += 8;

    table_name = read_32_be(&utf_packet[pos]);
    pos += 4;

    n_columns = read_16_be(&utf_packet[pos]);
    pos += 2;

    row_length = read_16_be(&utf_packet[pos]);
    pos += 2;

    n_rows = read_32_be(&utf_packet[pos]);
    pos += 4;

    for (uint16_t i = 0; i < n_columns; i++) {
        UTFColumn column;

        column.flags = utf_packet[pos];
        pos++;

        if (column.flags == '\0') {
            pos += 3;
            column.flags = utf_packet[pos];
            pos++;
        }

        uint32_t column_name_offset = read_32_be(&utf_packet[pos]);
        pos += 4;

        size_t column_name_length = strlen(reinterpret_cast<char*>(&utf_packet[column_name_offset + strings_offset]));

        column.name = new char[column_name_length + 1];
        memcpy_s(column.name, column_name_length + 1, &utf_packet[column_name_offset + strings_offset], column_name_length + 1);

        columns.push_back(column);
    }

    for (uint32_t j = 0; j < n_rows; j++) {
        pos = rows_offset + (j * row_length);

        UTFRows current_entry;

        for (uint16_t i = 0; i < n_columns; i++) {
            UTFRow current_row;
            
            uint32_t storage_flag = (columns[i].flags & STORAGE_MASK);

            if (storage_flag == STORAGE_NONE || storage_flag == STORAGE_ZERO || storage_flag == STORAGE_CONSTANT) {
                current_entry.rows.push_back(current_row);
                continue;
            }

            current_row.type = (columns[i].flags & TYPE_MASK);
            current_row.position = pos;

            switch (current_row.type) {
                case TYPE_1BYTE:
                case TYPE_1BYTE2:
                    current_row.val = QVariant::fromValue(utf_packet[pos]);
                    pos++;
                    break;

                case TYPE_2BYTE:
                case TYPE_2BYTE2:
                    current_row.val = QVariant::fromValue(read_16_be(&utf_packet[pos]));
                    pos += 2;
                    break;

                case TYPE_4BYTE:
                case TYPE_4BYTE2:
                    current_row.val = QVariant::fromValue(read_32_be(&utf_packet[pos]));
                    pos += 4;
                    break;

                case TYPE_8BYTE:
                case TYPE_8BYTE2:
                    current_row.val = QVariant::fromValue(read_64_be(&utf_packet[pos]));
                    pos += 8;
                    break;

                case TYPE_FLOAT:
                    current_row.val = QVariant::fromValue(read_float_be(&utf_packet[pos]));
                    pos += 4;
                    break;

                case TYPE_DATA: {
                        uint64_t position = read_32_be(&utf_packet[pos]) + data_offset;
                        pos += 4;

                        current_row.position = pos;
                        
                        uint32_t data_size = read_32_be(&utf_packet[pos]);
                        pos += 4;
                        char* buf = new char[data_size];

                        memcpy_s(buf, data_size, &utf_packet[position], data_size);

                        current_row.val = QVariant::fromValue(QByteArray(buf, data_size));
                        break;
                    }

                case TYPE_STRING: {
                        uint32_t str_offset = read_32_be(&utf_packet[pos]);
                        pos += 4;

                        size_t str_size = strlen(reinterpret_cast<char*>(&utf_packet[str_offset + strings_offset]));

                        char* str_val = new char[str_size + 1];

                        memcpy_s(str_val, str_size + 1, &utf_packet[str_offset + strings_offset], str_size + 1);

                        current_row.val = QVariant::fromValue(QString(str_val));
                    }

            }

            current_entry.rows.push_back(current_row);
        }

        rows.push_back(current_entry);
    }
}

CripackReader::DATReader::DATReader(char* file, std::map<uint32_t, QIcon>& file_icons) :
    file_icons(file_icons) {
    if (memcmp(file, "DAT\0", 4) != 0) {
        //throw FormatError("Invalid DAT header");
        throw FormatError(file);
    }

    file_count = read_32_le(reinterpret_cast<unsigned char*>(&file[4]));
    file_table_offset = read_32_le(reinterpret_cast<unsigned char*>(&file[8]));
    name_table_offset = read_32_le(reinterpret_cast<unsigned char*>(&file[16])) + 4U;
    size_table_offset = read_32_le(reinterpret_cast<unsigned char*>(&file[20]));
    name_align = read_32_le(reinterpret_cast<unsigned char*>(&file[name_table_offset - 4]));

    file_table.reserve(file_count);

    for (uint32_t i = 0; i < file_count; i++) {
        LightEmbeddedFile f;

        f.index = i;
        f.offset = read_32_le(reinterpret_cast<unsigned char*>(&file[file_table_offset + 4 * i]));
        f.size = read_32_le(reinterpret_cast<unsigned char*>(&file[size_table_offset + 4 * i]));

        uint32_t actual_name_size = strlen(&file[name_table_offset + name_align * i]);
        char* fname = new char[actual_name_size + 1];

        memcpy_s(fname, actual_name_size + 1, &file[name_table_offset + name_align * i], name_align);

        f.name = std::string(fname);

        delete fname;

        file_table.push_back(f);
    }
}

std::vector<QStandardItem*> CripackReader::DATReader::file_contents() {
    std::vector<QStandardItem*> result;
    
    for (LightEmbeddedFile f : file_table) {
        QStandardItem* file = new QStandardItem(f.name.c_str());
        file->setIcon(file_icons[NME2::fname_to_icon(f.name)]);
        file->setEditable(false);
        file->setData(QVariant::fromValue(f), NME2::EmbeddedRole);
        file->setData(QVariant::fromValue(this), NME2::ReaderRole);
        file->setData("DAT", NME2::ReaderTypeRole);

        result.push_back(file);
    }

    return result;
}