#pragma once

#include <qplaintextedit.h>
#include <qpainter.h>
#include <qtextobject.h>

class LineEditor : public QPlainTextEdit {
    Q_OBJECT

    public:
    explicit LineEditor();
    ~LineEditor() { }

    void line_number_paint_event(QPaintEvent* event);
    int line_number_width();

    protected:
    void resizeEvent(QResizeEvent* event) override;

    private slots:
    void update_line_number_width(int count);
    void update_line_number_area(const QRect &, int);

    private:
    class LineNumberArea : public QWidget {
        public:
        explicit LineNumberArea(LineEditor* editor) : QWidget(editor) {
            this->editor = editor;
        }

        QSize sizeHint() const override {
            return QSize(editor->line_number_width(), 0);
        }

        protected:
        void paintEvent(QPaintEvent* event) override {
            editor->line_number_paint_event(event);
        }

        private:
        LineEditor* editor;
    };

    QWidget* line_number_area;
};

