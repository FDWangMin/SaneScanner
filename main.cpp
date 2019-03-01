#include "myscanner.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MyScanner w;
    w.show();
	w.initScannerList();

    return a.exec();
}
