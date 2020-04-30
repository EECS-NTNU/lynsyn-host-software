QT += widgets sql
QMAKE_CXXFLAGS += -std=gnu++11 -Wno-unused-parameter -Wno-deprecated-copy

HEADERS = $$files(src/*.h, true)
SOURCES = $$files(src/*.cpp, true) ../liblynsyn/lynsyn.c

INCLUDEPATH += src /usr/include/libusb-1.0/ ../common/ ../liblynsyn/ /mingw64/include/libusb-1.0/

RESOURCES     = application.qrc

LIBS += -lusb-1.0

# install
target.path = /usr/bin/
INSTALLS += target

QMAKE_CXXFLAGS_RELEASE += -DNDEBUG

FORMS += \
    src/logdialog.ui \
    src/mainwindow.ui \
    src/profiledialog.ui \
    src/importdialog.ui \
    src/livedialog.ui
