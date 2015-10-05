#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
//    return 0;

    QApplication a(argc, argv);
    MainWindow w;

//    a.setStyle(QStyleFactory::create("Fusion"));


    w.show();

    return a.exec();
}
