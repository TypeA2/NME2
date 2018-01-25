#pragma once

#include <QtWidgets\QMainWindow>
#include <QtConcurrent\qtconcurrentrun.h>

#include <qfileiconprovider.h>
#include <qdiriterator.h>
#include <qboxlayout.h>
#include <qlabel.h>
#include <qfuturewatcher.h>
#include <qtemporaryfile.h>
#include <qheaderview.h>

#include <iostream>
#include <vector>

#include "NMETreeView.h"
#include "NMEItemModel.h"

class NME2 : public QMainWindow {
    Q_OBJECT

    public:
        explicit NME2(QString game_dir_path, std::initializer_list<QString> accepted_items);

        enum FileIconType : uint32_t {
            TypeNull,
            TypeAudio,
            TypeVideo,
            TypeImage,
            TypeText,
            TypeXML
        };

        enum ItemModelRoles {
            PathRole = Qt::UserRole + 1,
            FilenameRole,
            //TypeRole,
            ReaderRole,
            ReaderTypeRole
        };

        /*enum ItemTypes : uint8_t {
            TypeDir,
            TypeFile
        };*/

        template <typename T>
        static inline QList<T> vector_to_qlist(std::vector<T> vec) {
            QList<T> list;

            list.reserve(vec.size());

            std::copy(vec.begin(), vec.end(), std::back_inserter(list));

            return list;
        }

    private slots:
        void model_selection_changed(const QItemSelection & /*newSelection*/, const QItemSelection & /*oldSelection*/);

    private:
        QString game_dir_path;
        QStringList accepted_items;
        std::map<uint32_t, QIcon> file_icons;
        NMEItemModel* model;
        NMETreeView* view;

        QWidget* active_item_widget;
        QGridLayout* active_item_layout;

        void generate_file_icons();
        void create_tree_view();
        std::vector<QStandardItem*> iterate_directory(QString dir, QStringList filters);
        std::vector<QStandardItem*> scan_file_contents(QFileInfo &file);
};
