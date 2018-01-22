#pragma once

#include <qtreeview.h>

class NMETreeView : public QTreeView {
    Q_OBJECT

    public:
        NMETreeView(QWidget* parent = nullptr);

    protected:
        void keyPressEvent(QKeyEvent *event);
};

