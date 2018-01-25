#pragma once

#include "Error.h"
#include "BitManipulation.h"

#include <qfileinfo.h>
#include <qstandarditemmodel.h>

#include <fstream>

class WWRiffReader {
    public:
        explicit WWRiffReader(QFileInfo file, std::map<uint32_t, QIcon>& icons);
        ~WWRiffReader();

        std::vector<QStandardItem*> file_contents();

    private:
        std::ifstream infile;
        QFileInfo file;
        std::map<uint32_t, QIcon>& file_icons;

        struct WWRiffFile {
            uint64_t size;
            uint64_t offset;
        };

        std::vector<WWRiffFile> file_table;
};

Q_DECLARE_METATYPE(WWRiffReader*);