#include "WWRiffReader.h"

#include "NME2.h"

WWRiffReader::WWRiffReader(QFileInfo file, std::map<uint32_t, QIcon>& icons) :
    infile(file.canonicalFilePath().toStdString().c_str(), std::ios::binary | std::ios::in),
    file(file),
    file_icons(icons) {

    while (infile.tellg() < file.size()) {
        //infile.seekg(offset, std::ios::beg);

        WWRiffFile f;

        f.offset = infile.tellg();

        {
            char riff_hdr[4];
            char wave_hdr[4];

            infile.read(riff_hdr, 4);

            if (memcmp(&riff_hdr[0], "RIFF", 4) != 0) {
                std::cout << riff_hdr << std::endl;
                throw FormatError("Invalid RIFF signature");
            }

            f.size = (read_32_le(infile) + 8);

            infile.read(wave_hdr, 4);

            if (memcmp(wave_hdr, "WAVE", 4) != 0) {
                throw FormatError("Invalid WAVE signature");
            }
        }

        infile.seekg(f.size + 8, std::ios_base::cur);

        file_table.push_back(f);
    }
}

std::vector<QStandardItem*> WWRiffReader::file_contents() {
    std::vector<QStandardItem*> result;

    uint32_t i = 0;
    for (WWRiffFile f : file_table) {
        QStandardItem* item = new QStandardItem(file.baseName());
        item->setEditable(false);
        item->setIcon(file_icons[NME2::TypeAudio]);
        item->setData(QVariant::fromValue(this), NME2::ReaderRole);
        item->setData("WWRiff", NME2::ReaderTypeRole);

        result.push_back(item);
    }

    return result;
}

WWRiffReader::~WWRiffReader() {

}
