#pragma once

#include <QtWidgets\QMainWindow>
#include <QtConcurrent>

#include <QFileIconProvider>
#include <QDirIterator>
#include <QBoxLayout>
#include <QLabel>
#include <QFutureWatcher>
#include <QTemporaryFile>
#include <QHeaderView>
#include <QFormLayout>
#include <QPlaintextEdit>

#include <iostream>
#include <vector>

#include "NMETreeView.h"
#include "NMEItemModel.h"
#include "LineEditor.h"

class NME2 : public QMainWindow {
    Q_OBJECT

    public:
    explicit NME2(QString game_dir_path, std::initializer_list<QString> accepted_items);
    ~NME2() {

    }

    enum IconTypes : uint8_t {
        TypeNull,
        TypeAudio,
        TypeVideo,
        TypeImage,
        TypeText,
        TypeXML,
        TypePackage
    };

    enum ItemModelRoles {
        PathRole = Qt::UserRole + 1,
        FilenameRole,
        TypeFlag,
        ReaderRole,
        ReaderTypeRole,
        EmbeddedRole
    };

    enum ItemTypes : uint8_t {
        FileFlag,
        DirFlag
    };

    template <typename T>
    static inline QList<T> vector_to_qlist(std::vector<T> vec) {
        QList<T> list;

        list.reserve(vec.size());

        std::copy(vec.begin(), vec.end(), std::back_inserter(list));

        return list;
    }

    static inline uint8_t fname_to_icon(std::string fname) {
        if (ends_with(fname, ".wtp")) {
            return TypeImage;
        } else if (ends_with(fname, ".usm")) {
            return TypeVideo;
        } else if (ends_with(fname, ".wsp") || ends_with(fname, ".dat") || ends_with(fname, ".dtt") || ends_with(fname, ".cpk") || ends_with(fname, ".eff") || ends_with(fname, ".evn")) {
            return TypePackage;
        } else if (ends_with(fname, ".wem") || ends_with(fname, ".bnk")) {
            return TypeAudio;
        } else if(ends_with(fname, ".txt") || ends_with(fname, ".rss")){
            return TypeText;
        } else {
            return TypeNull;
        }
    }

    static inline bool ends_with(std::string const &v, std::string const &e) {
        if (e.size() > v.size()) return false;

        return std::equal(e.rbegin(), e.rend(), v.rbegin());
    }

    private slots:
    void model_selection_changed(const QItemSelection & /*newSelection*/, const QItemSelection & /*oldSelection*/);
    void check_model_selection(QModelIndex& selected);

    private:
    QString game_dir_path;
    QStringList accepted_items;
    std::map<uint32_t, QIcon> file_icons;
    NMEItemModel* model;
    NMETreeView* view;

    QWidget* active_item_widget;
    QVBoxLayout* active_item_layout;

    std::map<std::string, QString> txt_file_cache;
    QSize fullscreen_size_cache;
    QPoint fullscreen_pos_cache;
    QMargins fullscreen_margins_cache;
    QMargins active_layout_margins_cache;

    void generate_file_icons();
    void create_tree_view();
    std::vector<QStandardItem*> iterate_directory(QString dir, QStringList filters);
    std::vector<QStandardItem*> scan_file_contents(QFileInfo &file);
};
