#pragma once

#include <qstandarditemmodel.h>

class NMEItemModel : public QStandardItemModel {
    Q_OBJECT

    public:
        NMEItemModel();
        ~NMEItemModel();
};
