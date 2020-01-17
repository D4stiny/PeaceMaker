#include "mainwindow.h"

#include <Windows.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
