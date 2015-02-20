include(../p1load.pri)

TEMPLATE = lib
TARGET = ploader 
CONFIG += staticlib

SOURCES += \
    ../../ploader.c \
    ../../packet.c \
    ../../port.c \

HEADERS += \
    ../../ploader.h

unix:!macx {
    DEFINES += LINUX
    SOURCES += \
        ../../osint_linux.c

    equals(CPU, armhf) {
        DEFINES += RASPBERRY_PI
        SOURCES += \
            ../../gpio_sysfs.c
    }
}
macx {
    DEFINES += MACOSX
    SOURCES += \
        ../../osint_linux.c
}
win32 {
    DEFINES += MINGW
    LIBS    += -lsetupapi
    SOURCES += \
        ../../osint_mingw.c \
        ../../enumcom.c
}
