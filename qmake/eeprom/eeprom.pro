include(../p1load.pri)

TEMPLATE = app
TARGET = eeprom

LIBS += -L$${OUT_PWD}/../ploader/ -lploader

SOURCES += \
    ../../eeprom.c \
