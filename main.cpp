#include "signdialog.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SignDialog w;
    w.show();

    return a.exec();
}
