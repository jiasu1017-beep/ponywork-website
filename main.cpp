#include "mainwindow.h"
#include <QApplication>
#include <QTextCodec>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("小马办公");
    a.setOrganizationName("PonyWork");
    a.setApplicationVersion("1.0.3");
    
    QIcon appIcon(":/img/icon.png");
    a.setWindowIcon(appIcon);
    
    MainWindow w;
    w.show();
    
    return a.exec();
}
