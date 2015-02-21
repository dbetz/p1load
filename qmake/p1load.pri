!greaterThan(QT_MAJOR_VERSION, 4): {
    error("PropellerIDE requires Qt5.2 or greater")
}
!greaterThan(QT_MINOR_VERSION, 1): {
    error("PropellerIDE requires Qt5.2 or greater")
}

CONFIG -= qt debug_and_release app_bundle
CONFIG += console

INCLUDEPATH += ../..
