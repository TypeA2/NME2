#pragma once

#include <QSlider>
#include <QStyleOptionSlider>
#include <QMouseEvent>

class NMESlider : public QSlider {
    public:
    explicit NMESlider(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr) : QSlider(orientation, parent) { }
    ~NMESlider() { }

    protected:
    void mousePressEvent(QMouseEvent *event);
};

