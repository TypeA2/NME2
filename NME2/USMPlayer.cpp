#include "USMPlayer.h"

#include "BitManipulation.h"
#include "Error.h"

USMPlayer::USMPlayer(std::string fpath, std::map<uint32_t, QIcon>& icons, QWidget* parent) : QWidget(parent), CripackReader(infile, icons, false),
infile(fpath.c_str(), std::ios::binary | std::ios::in),
layout(new QGridLayout(this)),
play_pause_button(new QPushButton("Loading")),
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

    connect(play_pause_button, &QPushButton::released, this, &USMPlayer::toggle_play_state);

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

    QtAV::setLogLevel(QtAV::LogOff);
    QFutureWatcher<void>* watcher = new QFutureWatcher<void>(this);

    connect(watcher, &QFutureWatcher<void>::finished, this, [=] {
        video = buffers.at(index);
        stream = streams.at(index);

        player->setIODevice(video);

        progress_slider->setRange(0, stream.specs.total_frames);

        play_pause_button->setText("Pause");

        play_pause_button->setDisabled(false);
        progress_slider->setDisabled(false);

        position_modifier = 1000.L / (static_cast<long double>(stream.specs.framerate_n) / 1000.L);

        output->setRegionOfInterest(0, 0, streams[index].specs.disp_width, streams[index].specs.disp_height);
        output->setOutAspectRatio(static_cast<double>(stream.specs.disp_width) * static_cast<double>(stream.specs.disp_height));

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
    uint32_t streams_live = 0;
    uint32_t n_streams = 0;
    bool streams_ready = false;

    char* strtbl;

    do {
        uint32_t stmid = read_32_be(infile);
        uint32_t block_size = read_32_be(infile);
        uint32_t current_stream;

        if (!streams_ready) {
            if (stmid != 'CRID') {
                throw USMFormatError("Invalid CRID header");
            }
        } else {
            if (n_streams == 0) {
                throw USMFormatError("Expected at least 1 stream");
            }

            for (current_stream = 1; current_stream < n_streams; current_stream++) {
                if (stmid == streams[current_stream].stmid) {
                    break;
                }
            }

            if (current_stream == n_streams) {
                throw USMFormatError("Unknown stmid");
            }

            if (!streams[current_stream].alive) {
                throw USMFormatError("Stream should be alive");
            }

            streams[current_stream].bytes_read += block_size;
        }

        uint16_t header_size = read_16_be(infile);

        if (header_size != 0x18) {
            throw USMFormatError("Expected header size of 0x18");
        }

        uint16_t footer_size = read_16_be(infile);
        uint32_t payload_size = block_size - header_size - footer_size;
        uint32_t block_type = read_32_be(infile);

        infile.seekg(8, std::ios::cur);

        if (read_32_be(infile) != 0 || read_32_be(infile) != 0) {
            throw USMFormatError("Unkown byte format");
        }

        if (!streams_ready) {
            if (block_type != 1) {
                throw USMFormatError("CRID type should be 1");
            }

            uint64_t criusf_offset = infile.tellg();

            this->read_utf_noheader();
            UTF* utf = new UTF(reinterpret_cast<unsigned char*>(utf_packet));

            if (utf->n_rows < 1) {
                throw USMFormatError("Expected at least 1 row");
            }

            n_streams = utf->n_rows;

            uint64_t data_offset = criusf_offset + utf->get_data_offset();

            strtbl = utf->load_strtbl(infile, criusf_offset - 8);

            if (strcmp(&strtbl[utf->get_table_name()], "CRIUSF_DIR_STREAM") != 0) {
                throw USMFormatError("Expected CRIUSF_DRI_STREAM");
            }

            for (uint32_t i = 0; i < n_streams; i++) {
                Stream stream;
                stream.stmid = get_column_data(utf, i, "stmid").toUInt();
                stream.chno = get_column_data(utf, i, "chno").toUInt();
                stream.datasize = get_column_data(utf, i, "datasize").toULongLong();

                if (i == 0) {
                    if (stream.stmid != 0) {
                        throw USMFormatError("Expected identifier 0 for stream 0");
                    }

                    // 0xffffU == -1
                    if (stream.chno != 0xffff) {
                        throw USMFormatError("Expected chno -1 for stream 0");
                    }
                } else {
                    if (stream.stmid != '@SFV' && stream.stmid != '@SFA') {
                        throw USMFormatError("Unknown identifier");
                    }

                    if (stream.datasize != 0) {
                        throw USMFormatError("Expected datasize of 0");
                    }
                }

                streams.push_back(stream);
            }

            buffers.push_back(nullptr);
            streams[0].alive = false;

            for (uint32_t i = 1; i < n_streams; i++) {
                buffers.push_back(new QBuffer());
                buffers[i]->open(QIODevice::ReadWrite);

                streams[i].alive = true;
                streams[i].bytes_read = 0;
                streams[i].payload_size = 0;

                streams_live++;
            }

            streams_ready = true;

            infile.seekg(criusf_offset + payload_size, std::ios::beg);

            delete utf;
        } else {
            switch (block_type) {
                case 0: {
                        char* payload = new char[payload_size];

                        infile.read(payload, payload_size);

                        buffers[current_stream]->write(payload, payload_size);
                        streams[current_stream].payload_size += payload_size;

                        delete payload;

                        break;
                    }

                case 1: {
                        uint64_t start = infile.tellg();

                        this->read_utf_noheader();
                        UTF* utf = new UTF(reinterpret_cast<unsigned char*>(utf_packet));

                        StreamSpecs specs;
                        specs.disp_width = get_column_data(utf, 0, "disp_width").toUInt();
                        specs.disp_height = get_column_data(utf, 0, "disp_height").toUInt();
                        specs.total_frames = get_column_data(utf, 0, "total_frames").toUInt();
                        specs.framerate_n = get_column_data(utf, 0, "framerate_n").toUInt();

                        streams[current_stream].specs = specs;

                        delete utf;

                        break;
                    }
                case 3:
                    infile.seekg(payload_size, std::ios::cur);
                    break;

                case 2: {
                        char* payload = new char[payload_size];

                        infile.read(payload, payload_size);

                        if (strncmp(payload, "#CONTENTS END   ===============", payload_size) == 0) {
                            streams[current_stream].alive = false;

                            streams_live--;
                        }

                        delete payload;
                        break;
                    }

                default:
                    throw USMFormatError("Unknown block type");
            }
        }

        infile.seekg(footer_size, std::ios::cur);

    } while (streams_live > 0);

    for (uint32_t i = 0; i < buffers.size(); i++) {
        if (streams[i].stmid == '@SFV') {
            index = i;

            buffers[i]->seek(0);
            break;
        }
    }
}

void USMPlayer::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);

    output_widget->setMaximumHeight(floor(double(output->rendererWidth()) / double(streams[index].specs.disp_width) * double(streams[index].specs.disp_height)));

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