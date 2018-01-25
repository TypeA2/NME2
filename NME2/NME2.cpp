#include "NME2.h"

#include "NMEItemModel.h"
#include "NMETreeView.h"
#include "CripackReader.h"
#include "WWRiffReader.h"

NME2::NME2(QString game_dir_path, std::initializer_list<QString> accepted_items) : QMainWindow(), 
    game_dir_path(game_dir_path + "/data"),
    accepted_items(accepted_items), 
    model(new NMEItemModel()),
    view(new NMETreeView()) {

    view->header()->close();

    view->setAlternatingRowColors(true);

    this->generate_file_icons();
    this->resize(960, 540);
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
        std::vector<QStandardItem*> result = items_watcher->result();
        
        for (int32_t i = result.size() - 1; i >= 0; i--) {
            root_dir->appendRow(result[i]);
        }
        //root_dir->appendRows(vector_to_qlist<QStandardItem*>(items_watcher->result()));

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
        active_item_layout = new QGridLayout(active_item_widget);

        central_layout->addWidget(view, 0, 0);
        central_layout->addWidget(active_item_widget, 0, 1);

        central_layout->setColumnStretch(0, 1);
        central_layout->setColumnStretch(1, 1);

        this->setCentralWidget(central_widget);
    });

    QFuture<std::vector<QStandardItem*>> items_future = QtConcurrent::run(this, &NME2::iterate_directory, game_dir_path, accepted_items);

    items_watcher->setFuture(items_future);
}

std::vector<QStandardItem*> NME2::iterate_directory(QString path, QStringList filters) {
    std::vector<QStandardItem*> output;
    QDirIterator iterator(path, filters, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    
   while (iterator.hasNext()) {
        iterator.next();
        QFileInfo file = iterator.fileInfo();
        QStandardItem* item = new QStandardItem(file.fileName());

        try {
            QFileIconProvider icons;

            //file.isDir() ? item->setData(TypeDir, TypeRole) : item->setData(TypeFile, TypeRole);

            if (file.isDir()) {
                item->setIcon(icons.icon(QFileIconProvider::Folder));

                item->appendRows(vector_to_qlist<QStandardItem*>(iterate_directory(file.canonicalFilePath(), filters)));
            } else if (file.completeSuffix() == "cpk") {
                item->setIcon(file_icons[TypeNull]);

                item->appendRows(vector_to_qlist<QStandardItem*>(scan_file_contents(file)));
            } else if (file.completeSuffix() == "wsp" || file.completeSuffix() == "wem") {
                item->setIcon(file_icons[TypeAudio]);

                item->appendRows(vector_to_qlist<QStandardItem*>(scan_file_contents(file)));
            } else {
                item->setIcon(icons.icon(QFileIconProvider::File));
                item->setData(file.fileName(), FilenameRole);
            }
        } catch (const FormatError &e) {
            std::cerr << e.what() << std::endl;
        }

        item->setData(file.canonicalFilePath(), PathRole);
        item->setEditable(false);

        output.push_back(item);
    }

    return output;
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
    QModelIndex index = view->selectionModel()->currentIndex();

    QVariant path = index.data(PathRole);
    QVariant readerv = index.data(ReaderRole);
    QVariant readerTypev = index.data(ReaderTypeRole);

    if (path.isValid()) {
        std::cout << "Selected " << path.toString().toStdString() << std::endl;
    } else if (readerTypev.toString() == "Cripack") {
        CripackReader* reader = readerv.value<CripackReader*>();
    }
}