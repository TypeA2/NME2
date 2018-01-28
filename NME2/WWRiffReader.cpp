#include "WWRiffReader.h"

#include "NME2.h"

WWRiffReader::WWRiffReader(QFileInfo file, std::map<uint32_t, QIcon>& icons) :
    infile(file.canonicalFilePath().toStdString().c_str(), std::ios::binary | std::ios::in),
    file(file),
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

            f.size = (read_32_le(infile) + 8);
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
    infile.seekg(0, std::ios::beg);
}

std::vector<QStandardItem*> WWRiffReader::file_contents() {
    std::vector<QStandardItem*> result;

    uint32_t i = 0;
    for (WWRiffFile f : file_table) {
        QStandardItem* item = new QStandardItem(file.baseName() + "_" + QString::number(i++));
        item->setEditable(false);
        item->setIcon(file_icons[NME2::TypeAudio]);
        item->setData(QVariant::fromValue(this), NME2::ReaderRole);
        item->setData("WWRiff", NME2::ReaderTypeRole);

        result.push_back(item);
    }

    return result;
}
