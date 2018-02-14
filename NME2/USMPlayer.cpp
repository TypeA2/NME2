#include "USMPlayer.h"

#include "BitManipulation.h"
#include "Error.h"

USMPlayer::USMPlayer(std::string fpath, std::map<uint32_t, QIcon>& icons, QWidget* parent) : QWidget(parent), CripackReader(infile, icons, false),
infile(fpath.c_str(), std::ios::binary | std::ios::in),
layout(new QGridLayout(this)),
play_pause_button(new QPushButton("Loading")),
export_button(new QPushButton("Export")),
player(new QtAV::AVPlayer(this)),
output(new QtAV::VideoOutput(this)),
progress_slider(new NMESlider(Qt::Horizontal)),
slider_unit(1000),
infile_path(fpath) {
    qApp->installEventFilter(this);

    layout->setContentsMargins(0, 0, 0, 0);

    this->setLayout(layout);

    infile.seekg(0, std::ios::end);

    fsize = infile.tellg();

    infile.seekg(0, std::ios::beg);

    player->setRenderer(output);
    output->setQuality(QtAV::VideoRenderer::QualityBest);

    play_pause_button->setIcon(play_icon);

    QFile qss(":/style/Style.qss");
    qss.open(QIODevice::ReadOnly);
    progress_slider->setStyleSheet(qss.readAll().constData());
    qss.close();

    play_pause_button->setDisabled(true);
    progress_slider->setDisabled(true);
    export_button->setDisabled(true);

    connect(play_pause_button, &QPushButton::released, this, &USMPlayer::toggle_play_state);
    connect(export_button, &QPushButton::released, this, &USMPlayer::export_mpeg);

    connect(progress_slider, &NMESlider::sliderMoved, this, static_cast<void (USMPlayer::*)(int64_t)>(&USMPlayer::slider_seek));
    connect(progress_slider, &NMESlider::sliderPressed, this, static_cast<void (USMPlayer::*)(void)>(&USMPlayer::slider_seek));

    connect(player, &QtAV::AVPlayer::positionChanged, this, static_cast<void (USMPlayer::*)(int64_t)>(&USMPlayer::update_slider));
    connect(player, &QtAV::AVPlayer::started, this, static_cast<void (USMPlayer::*)(void)>(&USMPlayer::update_slider));
    connect(player, &QtAV::AVPlayer::stateChanged, this, [=](QtAV::AVPlayer::State state) {
        if (state == QtAV::AVPlayer::StoppedState) {
            video->seek(0);
            progress_slider->setValue(0);
            toggle_play_state();
        }
    });

    output_widget = output->widget();

    layout->addWidget(output_widget, 0, 0);
    layout->addWidget(progress_slider, 1, 0);
    layout->addWidget(play_pause_button, 2, 0);
    layout->addWidget(export_button, 3, 0);

    QtAV::setLogLevel(QtAV::LogOff);
    QFutureWatcher<void>* watcher = new QFutureWatcher<void>(this);

    connect(watcher, &QFutureWatcher<void>::finished, this, [=] {
        player->setIODevice(video);

        progress_slider->setRange(0, stream.total_frames);

        play_pause_button->setText("Pause");

        play_pause_button->setDisabled(false);
        progress_slider->setDisabled(false);
        export_button->setDisabled(false);

        position_modifier = 1000.L / (static_cast<long double>(stream.framerate_n) / 1000.L);

        output->setRegionOfInterest(0, 0, stream.disp_width, stream.disp_height);
        output->setOutAspectRatio(static_cast<double>(stream.disp_width) * static_cast<double>(stream.disp_height));

        std::unique_ptr<QMetaObject::Connection> pconn{ new QMetaObject::Connection };
        QMetaObject::Connection& conn = *pconn;
        conn = connect(player, &QtAV::AVPlayer::started, this, [=]() {
            QCoreApplication::postEvent(this, new QResizeEvent(this->size(), this->size()));

            disconnect(conn);
        });

        player->play();

        delete watcher;

    });

    QFuture<void> future = QtConcurrent::run(this, &USMPlayer::analyse);

    watcher->setFuture(future);
}

void USMPlayer::export_mpeg() {
    if (video && video->size() > 96) {
        QString outfile = QFileDialog::getSaveFileName(this, "Select output file", QString(infile_path.c_str()).replace(".usm", ".mpg"), "MPEG-2 ES (*.mpg)");
        if (!outfile.isEmpty()) {
            QtConcurrent::run([&]() {
                bool resume = false;
                if (player->state() == QtAV::AVPlayer::PlayingState) {
                    player->pause(true);
                    resume = true;
                }

                int64_t streampos = video->pos();

                video->seek(0);

                QFile out(outfile);

                out.open(QIODevice::WriteOnly);

                out.write(video->readAll());

                out.close();

                video->seek(streampos);

                if (resume) {
                    player->pause(false);
                }

                return nullptr; // Segfault without this, seriously?
            });
        }
    }
}

void USMPlayer::slider_seek(int64_t val) {
    if (player->isPlaying()) {
        player->setPosition(std::llroundl(val * position_modifier));
    }
}

void USMPlayer::slider_seek() {
    slider_seek(progress_slider->value());
}

void USMPlayer::update_slider(int64_t val) {
    if (player->isPaused() && play_pause_button->text() == "Pause")
        toggle_play_state();

    progress_slider->setValue(std::llroundl(player->position() / position_modifier));
}

void USMPlayer::update_slider() {
    update_slider(player->position());
}

void USMPlayer::analyse() {
    uint32_t n_streams = 0;
    
    {
        {
            char crid_hdr[4];

            infile.read(crid_hdr, 4);

            if (memcmp(crid_hdr, "CRID", 4) != 0) {
                throw USMFormatError("Invalid CRID header");
            }
        }

        uint32_t block_size = read_32_be(infile);
        uint16_t header_size = read_16_be(infile);

        if (header_size != 0x18) {
            throw USMFormatError("Expected CRID header size of 0x18");
        }

        uint16_t footer_size = read_16_be(infile);

        uint32_t payload_size = block_size - header_size - footer_size;
        uint32_t block_type = read_32_be(infile);

        if (block_type != 1) {
            throw USMFormatError("CRID type should be 1");
        }

        infile.seekg(8, std::ios::cur);

        if (read_32_be(infile) != 0 || read_32_be(infile) != 0) {
            throw USMFormatError("Unknown byte format");
        }

        infile.seekg(payload_size + footer_size, std::ios::cur);
    }
    
    bool finished = false;
    video = new QBuffer();
    video->open(QIODevice::ReadWrite);

    while (!finished) {
        if (read_32_be(infile) != '@SFV') {
            continue;
        }

        uint32_t size = read_32_be(infile);
        uint16_t header_size = read_16_be(infile);
        uint16_t footer_size = read_16_be(infile);
        uint32_t payload_size = size - header_size - footer_size;
        uint32_t type = read_32_be(infile);

        infile.seekg(16, std::ios::cur);

        switch (type) {
            case 0: { // Data
                    char* payload = new char[payload_size];

                    infile.read(payload, payload_size);

                    video->write(payload, payload_size);

                    delete payload;

                    break;
                }

            case 1: { // Metadata
                    uint64_t startpos = infile.tellg();

                    this->read_utf_noheader();
                    UTF* utf = new UTF(reinterpret_cast<unsigned char*>(utf_packet));

                    stream.disp_width = get_column_data(utf, 0, "disp_width").toUInt();
                    stream.disp_height = get_column_data(utf, 0, "disp_height").toUInt();
                    stream.total_frames = get_column_data(utf, 0, "total_frames").toUInt();
                    stream.framerate_n = get_column_data(utf, 0, "framerate_n").toUInt();

                    delete utf;
                    delete utf_packet; 

                    infile.seekg(startpos + payload_size, std::ios::beg);

                    break;
                }

            case 2: { // Metadata
                    char* payload = new char[payload_size];

                    infile.read(payload, payload_size);

                    if (strncmp(payload, "#CONTENTS END   ===============", payload_size) == 0) {
                        video->seek(0);

                        finished = true;
                    }

                    delete payload;
                    break;
                }

            case 3: // Header
                infile.seekg(payload_size, std::ios::cur);
                break;
        }

        infile.seekg(footer_size, std::ios::cur);
    }
}

void USMPlayer::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);

    output_widget->setMaximumHeight(floor(double(output->rendererWidth()) / double(stream.disp_width) * double(stream.disp_height)));

}

bool USMPlayer::eventFilter(QObject* obj, QEvent* event) {
    Q_UNUSED(obj);

    switch (event->type()) {
        case QEvent::KeyPress: {
                QKeyEvent* ke = static_cast<QKeyEvent*>(event);

                if (ke->key() == Qt::Key_Space) {
                    play_pause_button->click();

                    return true;

                    break;
                } else if (ke->key() == Qt::Key_Escape && fullscreen) {
                    if (!output_widget->rect().contains(output_widget->mapFromGlobal(QCursor::pos())) && fullscreen) {
                        fullscreen = false;

                        emit fullscreen_state_changed();
                    }
                } else {
                    return true;
                    
                    break;
                }
            }

        case QEvent::MouseButtonDblClick: {
                if (output_widget->rect().contains(output_widget->mapFromGlobal(QCursor::pos()))) {
                    fullscreen = !fullscreen;

                    emit fullscreen_state_changed();
                }

                if (fullscreen) {
                    progress_slider->hide();
                    play_pause_button->hide();

                    setCursor(Qt::BlankCursor);
                } else {
                    progress_slider->show();
                    play_pause_button->show();

                    unsetCursor();
                }

                return true;

                break;
            }

        default:
            return false;
    }
}

void USMPlayer::toggle_play_state() {
    if (!player->isPlaying()) {
        play_pause_button->setText("Pause");
        player->play();

        return;
    }

    play_pause_button->setText(player->isPaused() ? "Pause" : "Play");
    player->pause(!player->isPaused());
}