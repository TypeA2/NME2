#pragma once

#include "CripackReader.h"
#include "NMESlider.h"

#include <QWidget>
#include <QGridLayout>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QLabel>
#include <QMediaPlaylist>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QPushButton>
#include <QResizeEvent>

#include <QtAV\QtAV>

#include <math.h>

class USMPlayer : public QWidget, public CripackReader {
    Q_OBJECT

    public:
    explicit USMPlayer(std::string fpath, std::map<uint32_t, QIcon>& icons, QWidget* parent = nullptr);

    ~USMPlayer() {
        delete player;
        delete output;
        delete output_widget;
        delete play_pause_button;
        delete progress_slider;

        for (QBuffer* buffer : buffers) {
            delete buffer;
        }
    }

    signals:
    void fullscreen_state_changed();

    private slots:
    void slider_seek(int64_t val);
    void slider_seek();
    void update_slider(int64_t val);
    void update_slider();
    void toggle_play_state();

    protected:
    void resizeEvent(QResizeEvent* e);

    bool eventFilter(QObject* obj, QEvent* event);

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

    bool fullscreen = false;
    QTimer mmove_timer;

    std::ifstream infile;
    uint64_t fsize;

    std::string infile_path;

    QIcon play_icon;
    QIcon pause_icon;

    QGridLayout* layout;

    QtAV::AVPlayer* player;
    QtAV::VideoOutput* output;
    int64_t slider_unit;
    QWidget* output_widget;
    
    std::vector<Stream> streams;
    std::vector<QBuffer*> buffers;
    QBuffer* video;
    Stream stream;
    uint32_t index;

    long double position_modifier;

    QPushButton* play_pause_button;
    NMESlider* progress_slider;

    void analyse();
};