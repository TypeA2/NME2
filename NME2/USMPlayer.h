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

#include <QtAV/QtAV>

class USMPlayer : public QWidget, public CripackReader {
    Q_OBJECT

    public:
    explicit USMPlayer(std::string fpath, std::map<uint32_t, QIcon>& icons, QWidget* parent = nullptr);

    ~USMPlayer() {
        delete player;
        delete output;
        delete video_file;
        delete play_pause_button;
        delete progress_slider;
    }

    private slots:
    void slider_seek(int64_t val);
    void slider_seek();
    void update_slider(int64_t val);
    void update_slider();
    void update_slider_unit();

    private:
    std::ifstream infile;
    uint64_t fsize;
    QByteArray video;

    std::string infile_path;

    QIcon play_icon;
    QIcon pause_icon;

    QGridLayout* layout;

    QtAV::AVPlayer* player;
    QtAV::VideoOutput* output;
    int64_t slider_unit;
    
    QBuffer* video_file;

    QPushButton* play_pause_button;
    QSlider* progress_slider;

    QBuffer* analyse();

    struct StreamInfo {
        std::string fname;
        uint64_t fsize;
        uint64_t dsize;

        uint32_t stmid;
        uint16_t chno;
        uint16_t minchk;
        uint32_t minbuf;
        uint32_t avbps;

        uint64_t bytes_read;
        uint64_t payload_bytes;
        uint32_t alive;
    };
};