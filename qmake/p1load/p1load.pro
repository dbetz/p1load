include(../p1load.pri)

TEMPLATE = app
TARGET = p1load

LIBS += -L$${OUT_PWD}/../ploader/ -lploader

SOURCES += \
    ../../p1load.c \
