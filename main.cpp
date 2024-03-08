#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "3D_Calibratio_App_PC_FPGA_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    QList<QScreen*> screens = QGuiApplication::screens();
    QRect screenGeometry;
    QScreen *screen;
   if (screens.size() > 1) {
       screen = screens[1];
    }else{
       screen = screens[0];
   }
     screenGeometry = screen->geometry();


    MainWindow w;
    w.setGeometry(screenGeometry);
    w.show();
    return a.exec();
}
