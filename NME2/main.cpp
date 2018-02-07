/*
    https://forum.facepunch.com/f/fbx/qvrb/NieR-Automata-3D-models/1/
    http://forum.xentax.com/viewtopic.php?f=10&t=16011&start=0
    https://hydrogenaud.io/index.php/topic,64328.0.html
    https://gist.github.com/Wunkolo/213aa61eb0c874172aec97ebb8ab89c2
    https://github.com/blueskythlikesclouds/SonicAudioTools/wiki/CRI-UTF-Table-Format

*/

#include "NME2.h"

#include <QtWidgets/QApplication>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    NME2 w("F:/Steam/SteamApps/Common/NieRAutomata", { "*" });

    return a.exec();
}
