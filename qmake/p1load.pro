TEMPLATE  = subdirs

SUBDIRS = \
    ploader \
    eeprom \
    p1load \

eeprom.depends = ploader
p1load.depends = ploader
