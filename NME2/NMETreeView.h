#pragma once

#include <QTreeView>

class NMETreeView : public QTreeView {
    Q_OBJECT

    public:
        NMETreeView(QWidget* parent = nullptr);

    protected:
        void keyPressEvent(QKeyEvent *event);
};

