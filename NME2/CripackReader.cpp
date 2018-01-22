#include "CripackReader.h"

#include "NME2.h"
#include "BitManipulation.h"

/*#define SET_READER(item) \
item->setData(QVariant::fromValue(this), 259); \
item->setData("Cripack", 260);*/

CripackReader::CripackReader(QFileInfo file) : 
    infile(file.canonicalFilePath().toStdString().c_str()) {
    {
        char cpk_header[4];
        infile.read(cpk_header, 4);

        if (memcmp(cpk_header, "CPK ", 4) != 0) {
            throw FormatError("Invalid CPK header");
        }
    }

    unk1 = read_32_le(infile);
    utf_size = read_64_le(infile);
    utf_packet = new char[utf_size];

    infile.read(utf_packet, utf_size);

    if (memcmp(utf_packet, "@UTF", 4) != 0) {
        throw FormatError("Invalid @UTF header");
    }

    cpk_packet = utf_packet;

    EmbeddedFile cpak_entry;
    cpak_entry.file_name = "CPK_HDR";
    cpak_entry.file_offset_pos = static_cast<uint64_t>(infile.tellg()) + 0x10;
    cpak_entry.file_size = utf_size;
    cpak_entry.file_type = "CPK";

    file_table.push_back(cpak_entry);

    utf = new UTF(reinterpret_cast<unsigned char*>(utf_packet));

    for (uint32_t i = 0; i < utf->columns.size(); i++) {
        cpkdata[utf->columns[i].name] = utf->rows[0].rows[i].val;
    }

    TocOffset = get_column_data(0, "TocOffset").v64;
    uint64_t TocOffsetPos = get_column_position(0, "TocOffset");

    EtocOffset = get_column_data(0, "EtocOffset").v64;
    uint64_t EtocOffsetPos = get_column_position(0, "EtocOffset");

    ItocOffset = get_column_data(0, "ItocOffset").v64;
    uint64_t ItocOffsetPos = get_column_position(0, "ItocOffset");

    GtocOffset = get_column_data(0, "GtocOffset").v64;
    uint64_t GtocOffsetPos = get_column_position(0, "GtocOffset");

    ContentOffset = get_column_data(0, "ContentOffset");
    uint64_t ContentOffsetPos = get_column_data(0, "ContentOffset");

    file_table.push_back(EmbeddedFile("CONTENT_OFFSET", ContentOffset, ItocOffset, "CPK", "HDR"));

    uint32_t n_files = get_column_data(0, "Files").v32;
    uint16_t alignment = get_column_data(0, "Align").v16;

    if (TocOffset != 0) {
        file_table.push_back(EmbeddedFile("TOC_HDR", TocOffset, TocOffsetPos, "CPK", "HDR"));
    }

}

EmbeddedFile CripackReader::embedded

CripackReader::UTFRowValues CripackReader::get_column_data(uint32_t row, std::string name) {
    UTFRowValues result;

    for (uint16_t i = 0; i < utf->n_columns; i++) {
        if (utf->columns[i].name == name) {
            result = utf->rows[row].rows[i].val;
        }
    }

    return result;
}

uint64_t CripackReader::get_column_position(uint32_t row, std::string name) {
    uint64_t result = 0;

    for (uint16_t i = 0; i < utf->n_columns; i++) {
        if (utf->columns[i].name == name) {
            result = utf->rows[row].rows[i].position;
            break;
        }
    }

    return result;
}

/*CripackReader::CripackReader(QFileInfo file) :
    infile(C_STR(file.canonicalFilePath()), std::ios::binary),
    toc_entries(0),
    toc_strtbl(NULL),
    toc_offset(0) {

    {
        unsigned char cpk_header[4];
        infile.read(reinterpret_cast<char*>(cpk_header), 4);

        if (memcmp(cpk_header, "CPK ", 4) != 0) {
            throw FormatError("Invalid CPK header");
        }
    }
    
    toc_offset = query_8(0x10, 0, "TocOffset");
    uint64_t content_offset = query_8(0x10, 0, "ContentOffset");
    uint32_t cpk_header_count = query_4(0x10, 0, "Files");

    {
        unsigned char toc_header[4];

        infile.seekg(toc_offset, std::ios::beg);

        infile.read(reinterpret_cast<char*>(toc_header), 4);

        if (memcmp(toc_header, "TOC ", 4) != 0) {
            throw FormatError("Invalid TOC signature");
        }
    }

    toc_entries = query_utf(toc_offset + 0x10, NULL).rows;
    toc_strtbl = load_strtbl(toc_offset + 0x10);

    if (toc_entries != cpk_header_count) {
        throw FormatError("CpkHeader file count and TOC entry count do not match");
    }
}

std::vector<std::string> CripackReader::tocs() {
    std::vector<std::string> result;

#define CREATE(s) result.push_back(s + std::string(": ") + std::to_string(query_8(0x10, 0, s)))

    CREATE("TocOffset");
    CREATE("EtocOffset");
    CREATE("ItocOffset");
    CREATE("GtocOffset");
    CREATE("ContentOffset");

#undef CREATE

    return result;
}

CripackReader::query_result CripackReader::query_utf(uint64_t offset, CripackReader::query* query) {
    {
        char utf_sig[4];

        infile.seekg(offset, std::ios::beg);

        infile.read(utf_sig, 4);

        if (memcmp(utf_sig, "@UTF", 4) != 0) {
            throw FormatError("Invalid @UTF signature");
        }
    }

    table table_info;
    table_info.offset = offset;

    table_info.size = read_32_be(infile);
    table_info.schema_offset = 0x20;
    table_info.rows_offset = read_32_be(infile);
    table_info.strtbl_offset = read_32_be(infile);
    table_info.data_offset = read_32_be(infile);

    uint32_t table_name_str = read_32_be(infile);

    table_info.columns = read_16_be(infile);
    table_info.row_width = read_16_be(infile);
    table_info.rows = read_32_be(infile);

    const uint64_t strtbl_size = table_info.data_offset - table_info.strtbl_offset;

    char* strtbl = new char[strtbl_size + 1]();

    std::vector<column> schema(table_info.columns);

    for (uint16_t i = 0; i < table_info.columns; i++) {
        infile >> schema[i].type;
        schema[i].column_name = strtbl + read_32_be(infile);

        if ((schema[i].type & STORAGE_MASK) == STORAGE_CONSTANT) {
            schema[i].const_offset = infile.tellg();

            switch (schema[i].type & TYPE_MASK) {
                case TYPE_8BYTE:
                case TYPE_DATA:
                    infile.seekg(8, std::ios::cur);
                    break;

                case TYPE_STRING:
                case TYPE_FLOAT:
                case TYPE_4BYTE2:
                case TYPE_4BYTE:
                    infile.seekg(4, std::ios::cur);
                    break;

                case TYPE_2BYTE2:
                case TYPE_2BYTE:
                    infile.seekg(2, std::ios::cur);
                    break;

                case TYPE_1BYTE2:
                case TYPE_1BYTE:
                    infile.seekg(1, std::ios::cur);
                    break;

                default:
                    throw FormatError("Unknown column type");
            }
        }
    }

    table_info.schema = schema;

    
    table_info.strtbl = strtbl;

    infile.seekg(table_info.strtbl_offset + 8 + offset, std::ios::beg);
    infile.read(strtbl, strtbl_size);

    table_info.tbl_name = table_info.strtbl + table_name_str;

    query_result result;
    result.rows          = table_info.rows;
    result.name_offset   = table_name_str;
    result.strtbl_offset = table_info.strtbl_offset;
    result.data_offset = table_info.data_offset;

    if (query) {
        for (uint32_t i = 0; i < table_info.rows; i++) {
            if (i != query->index) {
                continue;
            }

            uint32_t row_offset      = table_info.offset + 8 + table_info.rows_offset + i * table_info.row_width;
            const uint32_t row_start = row_offset;

            for (uint16_t j = 0; j < table_info.columns; j++) {
                uint8_t type          = table_info.schema[j].type;
                uint64_t const_offset = table_info.schema[j].const_offset;
                bool is_const         = false;
                bool qthis = (i == query->index && !strcmp(table_info.schema[j].column_name, query->name));

                if (qthis) {
                    result.found = true;
                    result.type  = schema[j].type & TYPE_MASK;
                }

                switch (schema[j].type & STORAGE_MASK) {
                    case STORAGE_PERROW:
                        break;
                        
                    case STORAGE_CONSTANT:
                        is_const = true;
                        break;

                    case STORAGE_ZERO:
                        if (qthis) {
                            memset(&result.value, 0, sizeof(result.value));
                        }
                        continue;
                        break;

                    default:
                        throw FormatError("Unknown storage class");
                }

                uint64_t data_offset;
                uint8_t bytes_read;

                if (is_const) {
                    data_offset = const_offset;
                } else {
                    data_offset = row_offset;
                }

                infile.seekg(data_offset, std::ios::beg);

                switch (type & TYPE_MASK) {
                    case TYPE_STRING: {
                            uint32_t str_offset = read_32_be(infile);

                            if (qthis) {
                                result.value.vstring = str_offset;
                            }

                            bytes_read = 4;

                            break;
                        }
                    case TYPE_DATA: {
                            uint32_t data_offset = read_32_be(infile);
                            uint32_t data_size = read_32_be(infile);

                            if (qthis) {
                                result.value.vdata.offset = data_offset;
                                result.value.vdata.size = data_size;
                            }

                            bytes_read = 8;

                            break;
                        }

                    case TYPE_8BYTE: {
                            uint64_t val = read_64_be(infile);

                            if (qthis) {
                                result.value.vu64 = val;
                            }

                            bytes_read = 8;
                            
                            break;
                        }

                    case TYPE_4BYTE2:
                    case TYPE_4BYTE: {
                            uint32_t val = read_32_be(infile);

                            if (qthis) {
                                result.value.vu32 = val;
                            }

                            bytes_read = 4;
                            
                            break;
                        }

                    case TYPE_2BYTE2:
                    case TYPE_2BYTE: {
                            uint16_t val = read_16_be(infile);

                            if (qthis) {
                                result.value.vu16 = val;
                            }

                            bytes_read = 2;

                            break;
                        }
                    case TYPE_FLOAT: {
                            union {
                                float fv;
                                uint32_t iv;
                            } conv;

                            conv.iv = read_32_be(infile);

                            if (qthis) {
                                result.value.vfloat = conv.fv;
                            }

                            bytes_read = 4;

                            break;
                        }

                    case TYPE_1BYTE2:
                    case TYPE_1BYTE: {
                            uint8_t val;
                            infile >> val;

                            if (qthis) {
                                result.value.vu8 = val;
                            }

                            bytes_read = 1;

                            break;
                        }

                    default:
                        throw FormatError(QString("Unknown normal type %0").arg(type & TYPE_MASK));
                }

                if (!is_const) {
                    row_offset += bytes_read;
                }
            }

            if (row_offset - row_start != table_info.row_width) {
                throw FormatError("Row misaligned");
            }

            if (i >= query->index) {
                break;
            }
        }
    }

    delete strtbl;

    return result;
}

std::vector<QStandardItem*> CripackReader::file_contents(std::map<uint32_t, QIcon> file_icons) {
    std::vector<std::string> dirs;
    std::vector<file_tuple> files(toc_entries);

    for (uint64_t i = 0; i < toc_entries; i++) {
        const char* dname = query_string(toc_offset + 0x10, i, "DirName", toc_strtbl);
        const char* fname = query_string(toc_offset + 0x10, i, "FileName", toc_strtbl);
        uint32_t file_size = query_4(toc_offset + 0x10, i, "FileSize");
        uint32_t extract_size = query_4(toc_offset + 0x10, i, "ExtractSize");

        if (std::find(dirs.begin(), dirs.end(), std::string(dname)) == dirs.end() && strlen(dname) != 0) {
            dirs.push_back(std::string(dname));
        }

        files[i] = std::make_tuple(std::string(dname), std::string(fname), file_size, extract_size);
    }

    return merge_dirs(dirs, files);
}

std::vector<QStandardItem*> CripackReader::merge_dirs(std::vector<std::string> dirs, std::vector<file_tuple> files) {
    std::vector<QStandardItem*> result;
    std::vector<QStandardItem*> container;
    std::map<std::string, QStandardItem*> dir_map;

    for (file_tuple file : files){
        std::string dir = std::get<0>(file);

        if (dir == "") {
            std::string fname = std::get<1>(file);
            QStandardItem* subfile = new QStandardItem(fname.c_str());
            subfile->setEditable(false);
            subfile->setIcon(fname_to_icon(fname));

            SET_READER(subfile);

            container.push_back(subfile);
        } else {
            std::string base_dir = dir.substr(0, dir.find('/'));

            if (!dir_map.count(base_dir)) {
                QStandardItem* dir_item = new QStandardItem(base_dir.c_str());
                dir_item->setIcon(QFileIconProvider().icon(QFileIconProvider::Folder));
                dir_item->setEditable(false);

                SET_READER(dir_item);

                dir_item->appendRows(NME2::vector_to_qlist<QStandardItem*>(match_dirs(base_dir, files)));

                dir_map[base_dir] = dir_item;
            }
        }
    }

    for (std::pair<std::string, QStandardItem*> dir : dir_map) {
        result.push_back(dir.second);
    }

    result.insert(result.end(), container.begin(), container.end());

    return result;
}

std::vector<QStandardItem*> CripackReader::match_dirs(std::string dir, std::vector<file_tuple> files) {
    std::vector<QStandardItem*> result;
    std::vector<QStandardItem*> container;
    std::map<std::string, QStandardItem*> dirs_map;
    bool dirs_present = false;

    for (file_tuple file : files) {
        if (std::get<0>(file) == dir) {
            std::string fname = std::get<1>(file);
            QStandardItem* subfile = new QStandardItem(fname.c_str());
            subfile->setEditable(false);
            subfile->setIcon(fname_to_icon(fname));

            SET_READER(subfile);

            container.push_back(subfile);
        } else if (std::get<0>(file).compare(0, dir.length(), dir) == 0) {
            dirs_present = true;

            std::string dirname = std::get<0>(file).substr(std::get<0>(file).find('/') + 1);
            
            if (!dirs_map.count(dirname)) {
                QStandardItem* subdir = new QStandardItem(dirname.c_str());
                subdir->setIcon(QFileIconProvider().icon(QFileIconProvider::Folder));
                subdir->setEditable(false);
                
                SET_READER(subdir);

                subdir->appendRows(NME2::vector_to_qlist<QStandardItem*>(match_dirs(dir + "/" + dirname, files)));

                dirs_map[dirname] = subdir;
            }
        } 
    }

    if (dirs_present) {
        for (std::pair<std::string, QStandardItem*> dir_pair : dirs_map) {
            result.push_back(dir_pair.second);
        }
    }

    result.insert(result.end(), container.begin(), container.end());

    return result;
}

QIcon CripackReader::fname_to_icon(std::string fname) {
    if (ends_with(fname, ".wtp") || ends_with(fname, ".dtt")) {
        return file_icons[NME2::TypeImage];
    } else if (ends_with(fname, ".usm")) {
        return file_icons[NME2::TypeVideo];
    } else if (ends_with(fname, ".wsp") || ends_with(fname, ".wem") || ends_with(fname, ".bnk")) {
        return file_icons[NME2::TypeAudio];
    } else {
        return file_icons[NME2::TypeNull];
    }
}

#undef SET_READER*/

CripackReader::UTF::UTF(unsigned char* utf_packet) : utf_packet(utf_packet), pos(4) {
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

    for (uint32_t i = 0; i < n_columns; i++) {
        struct UTFColumn column;

        column.flags = utf_packet[pos];
        pos++;

        if (column.flags == 0) {
            pos += 3;
            column.flags = utf_packet[pos];
            pos++;
        }

        uint32_t column_name_length = read_32_be(&utf_packet[pos]);
        pos += 4;

        char* column_name = new char[column_name_length + strings_offset];
        memcpy_s(column_name, column_name_length + strings_offset, &utf_packet[pos], column_name_length + strings_offset);
        pos += (column_name_length + strings_offset);

        column.name = column_name;

        columns.push_back(column);
    }

    for (uint32_t j = 0; j < n_rows; j++) {
        pos = rows_offset + (j * row_length);

        struct UTFRows current_entry;

        for (uint32_t i = 0; i < n_columns; i++) {
            struct UTFRow current_row;
            
            uint32_t storage_flag = (columns[i].flags & STORAGE_MASK);

            if (storage_flag == STORAGE_NONE || storage_flag == STORAGE_ZERO || storage_flag == STORAGE_CONSTANT) {
                current_entry.rows.push_back(current_row);
                continue;
            }

            current_row.type = columns[i].flags & TYPE_MASK;
            current_row.position = pos;

            switch (current_row.type) {
                case TYPE_1BYTE:
                case TYPE_1BYTE2:
                    current_row.val.v8 = utf_packet[pos];
                    pos++;
                    break;

                case TYPE_2BYTE:
                case TYPE_2BYTE2:
                    current_row.val.v16 = read_16_be(&utf_packet[pos]);
                    pos += 2;
                    break;

                case TYPE_4BYTE:
                case TYPE_4BYTE2:
                    current_row.val.v32 = read_32_be(&utf_packet[pos]);
                    pos += 4;
                    break;

                case TYPE_8BYTE:
                case TYPE_8BYTE2:
                    current_row.val.v64 = read_64_be(&utf_packet[pos]);
                    pos += 8;
                    break;

                case TYPE_FLOAT:
                    current_row.val.vfloat = read_float_be(&utf_packet[pos]);
                    pos += 4;
                    break;

                case TYPE_DATA: {
                        uint64_t position = read_32_be(&utf_packet[pos]) + data_offset;
                        pos += 4;

                        current_row.position = pos;
                        
                        uint32_t data_size = read_32_be(&utf_packet[pos]);
                        pos += 4;
                        current_row.val.data = new char[data_size];

                        memcpy_s(current_row.val.data, data_size, &utf_packet[position], data_size);
                        break;
                    }
            }

            current_entry.rows.push_back(current_row);
        }

        rows.push_back(current_entry);
    }
}

CripackReader::~CripackReader() {
    infile.close();
}