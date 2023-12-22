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
   // 假设您要在第二个屏幕上显示窗口（索引1）
   if (screens.size() > 1) {
       screen = screens[1]; // 这里使用索引1来选择第二个屏幕
    }else{
       screen = screens[0]; // 这里使用索引1来选择第二个屏幕
   }
     screenGeometry = screen->geometry();


    MainWindow w;
    w.setGeometry(screenGeometry);
    w.show();
    return a.exec();
}
