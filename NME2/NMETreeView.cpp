#include "NMETreeView.h"

#include <Qt3DInput\QKeyEvent>

NMETreeView::NMETreeView(QWidget* parent) : QTreeView(parent) {

}

void NMETreeView::keyPressEvent(QKeyEvent *event) {
    QTreeView::keyPressEvent(event);

    const int key = event->key();

    if (key == Qt::Key_Down || key == Qt::Key_Up ||
        key == Qt::Key_Left || key == Qt::Key_Right ||
        key == Qt::Key_W || key == Qt::Key_S ||
        key == Qt::Key_A || key == Qt::Key_D) {
        emit clicked(this->currentIndex());
    }
}