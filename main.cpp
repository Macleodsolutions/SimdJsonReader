#include "JsonReader.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    JsonReader w;
    w.show();
    return a.exec();
}
