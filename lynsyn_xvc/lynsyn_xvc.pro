CONFIG += console

QT += network
QMAKE_CXXFLAGS += -std=gnu++11 -Wno-unused-parameter

HEADERS = $$files(src/*.h, true)
SOURCES = $$files(src/*.cpp, true) ../liblynsyn/lynsyn.c

INCLUDEPATH += src /usr/include/libusb-1.0/ ../common/ ../liblynsyn/ /mingw64/include/libusb-1.0/

LIBS += -lusb-1.0

# install
target.path = /usr/bin/
INSTALLS += target

QMAKE_CXXFLAGS_RELEASE += -DNDEBUG
