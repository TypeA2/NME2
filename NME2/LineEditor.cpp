#include "LineEditor.h"

LineEditor::LineEditor() : QPlainTextEdit() {
    line_number_area = new LineNumberArea(this);

    connect(this, &LineEditor::blockCountChanged, this, &LineEditor::update_line_number_width);
    connect(this, &LineEditor::updateRequest, this, &LineEditor::update_line_number_area);

    update_line_number_width(0);
}

int LineEditor::line_number_width() {
    int digits = 1;
    int max = qMax(1, blockCount());

    while (max >= 10) {
        max /= 10;
        digits++;
    }

    return 5 + fontMetrics().width(QLatin1Char('9')) * digits;
}

void LineEditor::update_line_number_width(int /*count*/) {
    setViewportMargins(line_number_width(), 0, 0, 0);
}

void LineEditor::update_line_number_area(const QRect& rect, int dy) {
    if (dy) {
        line_number_area->scroll(0, dy);
    } else {
        line_number_area->update(0, rect.y(), line_number_area->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        update_line_number_width(0);
    }
}

void LineEditor::resizeEvent(QResizeEvent* e) {
    QPlainTextEdit::resizeEvent(e);

    QRect rect = contentsRect();

    line_number_area->setGeometry(QRect(rect.left(), rect.top(), line_number_width(), rect.height()));
}

void LineEditor::line_number_paint_event(QPaintEvent* event) {
    QPainter painter(line_number_area);

    painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int block_number = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingGeometry(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(Qt::black);
            painter.drawText(0, top, line_number_area->width() - 2, fontMetrics().height(), Qt::AlignRight, QString::number(block_number + 1));
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingGeometry(block).height());
        block_number++;
    }
}