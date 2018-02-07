#pragma once

#include "CripackReader.h"

#include <QWidget>
#include <QGridLayout>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QLabel>
#include <QMediaPlaylist>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QPushButton>
#include <QSlider>
#include <QResizeEvent>

#include <QtAV/QtAV>

#include <math.h>

class USMPLayerVideoOutputPrivate : public QtAV::VideoOutput {
    Q_OBJECT

    public:
    explicit USMPLayerVideoOutputPrivate(QWidget* parent = nullptr) : QtAV::VideoOutput(parent) { }

    signals:
    void toggle_fullscreen();

    protected:
    void mouseDoubleClickEvent(QMouseEvent* e) {
        qDebug() << "doubleclick";
    }
};

class USMPlayer : public QWidget, public CripackReader {
    Q_OBJECT

    public:
    explicit USMPlayer(std::string fpath, std::map<uint32_t, QIcon>& icons, QWidget* parent = nullptr);

    ~USMPlayer() {
        delete player;
        delete output;
        delete video;
        delete play_pause_button;
        delete progress_slider;
    }

    private slots:
    void slider_seek(int64_t val);
    void slider_seek();
    void update_slider(int64_t val);
    void update_slider();

    protected:
    void resizeEvent(QResizeEvent* e);
    void mouseDoubleClickEvent(QMouseEvent* e) {
        layout->itemAt(0)->widget()->showFullScreen();
    }

    private:
    
    struct StreamSpecs {
        uint32_t disp_width;
        uint32_t disp_height;
        uint32_t total_frames;
        uint32_t framerate_n;
    };

    struct Stream {
        bool alive;
        uint32_t stmid;
        uint16_t chno;
        uint64_t datasize;
        uint64_t bytes_read;
        uint64_t payload_size;

        StreamSpecs specs;
    };

    std::ifstream infile;
    uint64_t fsize;

    std::string infile_path;

    QIcon play_icon;
    QIcon pause_icon;

    QGridLayout* layout;

    QtAV::AVPlayer* player;
    USMPLayerVideoOutputPrivate* output;
    int64_t slider_unit;
    
    std::vector<Stream> streams;
    std::vector<QBuffer*> buffers;
    QBuffer* video;
    Stream stream;
    uint32_t index;

    long double position_modifier;

    QPushButton* play_pause_button;
    QSlider* progress_slider;

    void analyse();
};