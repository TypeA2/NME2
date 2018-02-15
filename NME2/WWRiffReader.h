#pragma once

#include "Error.h"
#include "BitManipulation.h"

#include <QWidget>
#include <QFileInfo>
#include <QStandardItemModel>

#include <fstream>

class WWRiffReader : public QWidget {
    Q_OBJECT

    public:
    explicit WWRiffReader(std::string fpath, std::map<uint32_t, QIcon>& icons, uint64_t index = 0, bool play = false);
    ~WWRiffReader() {
        infile.close();
    }

    struct WWRiffFile {
        uint64_t size;
        uint64_t offset;
    };

    std::vector<QStandardItem*> file_contents();

    private:
    std::ifstream infile;
    QFileInfo file;
    std::map<uint32_t, QIcon>& file_icons;

    

    std::vector<WWRiffFile> file_table;
};

Q_DECLARE_METATYPE(WWRiffReader*);