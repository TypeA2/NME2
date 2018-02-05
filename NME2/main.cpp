#include "NME2.h"

#include <Windows.h>
#include <QtWidgets/QApplication>
#include <QObject>

#include <QMessageBox>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    if (system("where ffmpeg > nul 2>&1") != 0) {
        QMessageBox err;
        err.setText("ffmpeg was not found in your PATH.");
        err.setIcon(QMessageBox::Critical);
        err.addButton("Close", QMessageBox::AcceptRole);
        err.exec();

        return 1;
    }

    NME2 w("F:/Steam/SteamApps/Common/NieRAutomata", { "*" });

    return a.exec();
}
