QT       += core gui
QT_DEBUG_PLUGINS=1

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++14

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    Calibrate_Process.cpp \
    DeviceEnumerator.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    AUO3DLib/include/AUO3DLib.h \
    Calibrate_Process.h \
    DeviceEnumerator.h \
    FPGA/AUO3DLib_FPGA.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    3D_Calibratio_App_PC_FPGA_zh_TW.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    AUO3DLib/3D content/left.bmp \
    AUO3DLib/3D content/right.bmp \
    AUO3DLib/include/AUO3DLib.dll \
    AUO3DLib/lib/AUO3DLib.lib \
    AUO3DLib/other/AUO3DLib_Uart.dll \
    AUO3DLib/other/AUOETKLib.dll \
    AUO3DLib/other/HxDongleDll.dll \
    AUO3DLib/other/OpenCL.dll \
    AUO3DLib/other/concrt140.dll \
    AUO3DLib/other/freeglut.dll \
    AUO3DLib/other/freetype.dll \
    AUO3DLib/other/freetype28.dll \
    AUO3DLib/other/glew32.dll \
    AUO3DLib/other/glfw3.dll \
    AUO3DLib/other/msvcp140.dll \
    AUO3DLib/other/msvcr100.dll \
    AUO3DLib/other/opencv_world412.dll \
    AUO3DLib/other/pthreadVC2.dll \
    AUO3DLib/other/python37.dll \
    AUO3DLib/other/realsense2.dll \
    AUO3DLib/other/vcruntime140.dll \
    AUO3DLib/other/vcruntime140_1.dll \
    AUO3DLib/other/vidcap.dll \
    FPGA.png \
    FPGA/AUO3DLib_FPGA.dll \
    FPGA/AUO3DLib_FPGA.lib \
    FPGA/EMMC - 3D parameters.ini \
    FPGA_clear.png \
    PC.png \
    PC_clear.png

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/AUO3DLib/lib/ -lAUO3DLib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/AUO3DLib/lib/ -lAUO3DLibd
else:unix: LIBS += -L$$PWD/AUO3DLib/lib/ -lAUO3DLib

INCLUDEPATH += $$PWD/AUO3DLib/include
DEPENDPATH += $$PWD/AUO3DLib/include

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/FPGA/ -lAUO3DLib_FPGA
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/FPGA/ -lAUO3DLib_FPGAd
else:unix: LIBS += -L$$PWD/FPGA/ -lAUO3DLib_FPGA

INCLUDEPATH += $$PWD/FPGA
DEPENDPATH += $$PWD/FPGA


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/opencv/build/x64/vc16/lib/ -lopencv_world470
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/opencv/build/x64/vc16/lib/ -lopencv_world470d
else:unix: LIBS += -L$$PWD/opencv/build/x64/vc16/lib/ -lopencv_world470

INCLUDEPATH += $$PWD/opencv/build/include
DEPENDPATH += $$PWD/opencv/build/include


unix|win32: LIBS += -lstrmiids
unix|win32: LIBS += -lOle32
unix|win32: LIBS += -lOleAut32
