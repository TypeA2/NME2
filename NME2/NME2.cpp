#include "NME2.h"

#include "NMEItemModel.h"
#include "NMETreeView.h"
#include "CripackReader.h"
#include "WWRiffReader.h"
#include "USMPlayer.h"

NME2::NME2(QString game_dir_path, std::initializer_list<QString> accepted_items) : QMainWindow(), 
    game_dir_path(game_dir_path + "/data"),
    accepted_items(accepted_items), 
    model(new NMEItemModel()),
    view(new NMETreeView()) {

    view->header()->close();

    view->setAlternatingRowColors(true);

    this->generate_file_icons();
    this->resize(1440, 810);
    this->show();

    this->create_tree_view();
}

void NME2::generate_file_icons() {
#define CREATE_ICON(e, t) { QTemporaryFile f(QDir::tempPath() + "XXXXXX." + e); \
        f.open();file_icons[t] = QIcon(provider.icon(QFileInfo(QDir::tempPath() + f.fileName())).pixmap(QSize(16, 16))); }
    QFileIconProvider provider;

    CREATE_ICON("nme2", TypeNull);
    CREATE_ICON("wav", TypeAudio);
    CREATE_ICON("mpeg", TypeVideo);
    CREATE_ICON("tiff", TypeImage);
    CREATE_ICON("txt", TypeText);
    CREATE_ICON("xml", TypeXML);
    CREATE_ICON("zip", TypePackage);

#undef CREATE_ICON
}

void NME2::create_tree_view() {
    QWidget* loading_widget = new QWidget(this);
    QVBoxLayout* loading_layout = new QVBoxLayout(loading_widget);
    QLabel* loading_label = new QLabel("Loading files...");

    loading_layout->addWidget(loading_label);
    loading_widget->setLayout(loading_layout);

    this->setCentralWidget(loading_widget);

    QStandardItem* root_dir = new QStandardItem(QDir(game_dir_path).dirName());
    root_dir->setEditable(false);
    root_dir->setData(game_dir_path, PathRole);
    root_dir->setIcon(QFileIconProvider().icon(QFileIconProvider::Folder));

    QFutureWatcher<std::vector<QStandardItem*>>* items_watcher = new QFutureWatcher<std::vector<QStandardItem*>>();

    connect(items_watcher, &QFutureWatcher<std::vector<QStandardItem*>>::finished, this, [=]() {
        root_dir->appendRows(vector_to_qlist<QStandardItem*>(items_watcher->result()));

        model->invisibleRootItem()->appendRow(root_dir);
        view->setModel(model);

        view->setExpanded(root_dir->index(), true);

        QItemSelectionModel* selectionModel = view->selectionModel();
        connect(selectionModel, &QItemSelectionModel::selectionChanged, this, &NME2::model_selection_changed);
        
        this->takeCentralWidget();

        delete loading_label;
        delete loading_layout;
        delete loading_widget;

        QWidget* central_widget = new QWidget(this);
        QGridLayout* central_layout = new QGridLayout(central_widget);
        active_item_widget = new QWidget(central_widget);
        active_item_layout = new QVBoxLayout(active_item_widget);

        central_layout->addWidget(view, 0, 0);
        central_layout->addWidget(active_item_widget, 0, 1);

        central_layout->setColumnStretch(0, 1);
        central_layout->setColumnStretch(1, 1);

        this->setCentralWidget(central_widget);

        delete items_watcher;
    });

    QFuture<std::vector<QStandardItem*>> items_future = QtConcurrent::run(this, &NME2::iterate_directory, game_dir_path, accepted_items);

    items_watcher->setFuture(items_future);
}

std::vector<QStandardItem*> NME2::iterate_directory(QString path, QStringList filters) {
    std::vector<QStandardItem*> output;
    std::vector<QStandardItem*> result;
    QDirIterator iterator(path, filters, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    
   while (iterator.hasNext()) {
        iterator.next();
        QFileInfo file = iterator.fileInfo();
        QStandardItem* item = new QStandardItem(file.fileName());

        try {
            QFileIconProvider icons;

            if (file.isDir()) {
                item->setData(DirFlag, TypeFlag);
                item->setIcon(icons.icon(QFileIconProvider::Folder));

                item->appendRows(vector_to_qlist<QStandardItem*>(iterate_directory(file.canonicalFilePath(), filters)));

            } else {
                item->setData(FileFlag, TypeFlag);
                item->setData(file.fileName(), FilenameRole);
                item->setIcon(file_icons[fname_to_icon(file.fileName().toStdString())]);

                if (file.completeSuffix() == "cpk") {
                    item->appendRows(vector_to_qlist<QStandardItem*>(scan_file_contents(file)));
                } else if (file.completeSuffix() == "wsp") {
                    std::vector<QStandardItem*> children = scan_file_contents(file);
                    item->appendRows(vector_to_qlist<QStandardItem*>(children));

                    item->setText(item->text() + QString(" (%0 entr%2)").arg(children.size()).arg((children.size() > 1) ? "ies" : "y"));
                }
            }
        } catch (const FormatError &e) {
            std::cerr << e.what() << std::endl;
        }

        item->setData(file.canonicalFilePath(), PathRole);
        item->setEditable(false);

        output.push_back(item);
    }

   for (QStandardItem* item : output) {
       if (item->data(TypeFlag).toInt() == DirFlag) {
           result.push_back(item);
       }
   }

   for (QStandardItem* item : output) {
       if (item->data(TypeFlag).toInt() == FileFlag) {
           result.push_back(item);
       }
   }

    return result;
}

std::vector<QStandardItem*> NME2::scan_file_contents(QFileInfo &file) {
    if (file.completeSuffix() == "cpk") {
        CripackReader* reader = new CripackReader(file, file_icons);

        return reader->file_contents();
    } else if (file.completeSuffix() == "wsp" || file.completeSuffix() == "wem") {
        WWRiffReader* reader = new WWRiffReader(file, file_icons);

        return reader->file_contents();
    }

    return std::vector<QStandardItem*>();
}

void NME2::model_selection_changed(const QItemSelection & /*newSelection*/, const QItemSelection & /*oldSelection*/) {

    QLayoutItem* child;
    while ((child = active_item_layout->takeAt(0)) != 0) {
        delete child->widget();
        delete child;
    }

    QModelIndex selected = view->selectionModel()->currentIndex();

    check_model_selection(selected);
}

void NME2::check_model_selection(QModelIndex& selected) {
    if (selected.data(PathRole).isValid()) {
        std::string fname = selected.data(FilenameRole).toString().toStdString();
        std::string path = selected.data(PathRole).toString().toStdString();

        if (ends_with(fname, ".rss")) {
            QFile infile(selected.data(PathRole).toString());

            if (infile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                LineEditor* editor = new LineEditor();
                editor->setReadOnly(true);

                active_item_layout->addWidget(editor);

                if (txt_file_cache.count(path) < 1) {
                    QString text;
                    while (!infile.atEnd()) {
                        text.append(infile.readLine());
                    }

                    txt_file_cache[path] = text;
                }

                editor->setPlainText(txt_file_cache[path]);
                editor->moveCursor(QTextCursor::Start);
            }
        } else if (ends_with(fname, ".usm")) {
            try {
                USMPlayer* player = new USMPlayer(path, file_icons);

                connect(player, &USMPlayer::fullscreen_state_changed, this, [=]() {
                    QGridLayout* layout = static_cast<QGridLayout*>(centralWidget()->layout());
                    if (windowState() & Qt::WindowFullScreen) {
                        this->showNormal();
                        
                        layout->setContentsMargins(fullscreen_margins_cache);
                        active_item_layout->setContentsMargins(active_layout_margins_cache);

                        view->show();

                        layout->removeWidget(active_item_widget);
                        
                        layout->addWidget(view, 0, 0);
                        layout->addWidget(active_item_widget, 0, 1);

                        this->resize(fullscreen_size_cache);
                        this->move(fullscreen_pos_cache);
                    } else {
                        fullscreen_size_cache = this->size();
                        fullscreen_pos_cache = this->pos();

                        fullscreen_margins_cache = layout->contentsMargins();
                        active_layout_margins_cache = active_item_layout->contentsMargins();

                        layout->setContentsMargins(0, 0, 0, 0);
                        active_item_layout->setContentsMargins(0, 0, 0, 0);

                        layout->removeWidget(view);
                        layout->removeWidget(active_item_widget);

                        view->hide();

                        layout->addWidget(active_item_widget, 0, 0, 1, 2);

                        this->showFullScreen();
                    }
                });

                active_item_layout->addWidget(player);
            } catch (const USMFormatError& e) {
                std::cout << "USMPlayer error: " << e.what() << std::endl;
            }
        }
    } else if (selected.data(ReaderTypeRole).toString() == "Cripack") {
        QVariant data = selected.data(EmbeddedRole);
        if (!data.isValid()) {
            std::cout << "Dir" << std::endl;
        } else {
            CripackReader::EmbeddedFile file = data.value<CripackReader::EmbeddedFile>();

            std::cout << file.file_size << " " << file.extract_size << std::endl;
        }
    }
}