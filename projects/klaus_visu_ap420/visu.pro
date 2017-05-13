#-------------------------------------------------
#
# Project created by QtCreator 2015-07-29T22:27:37
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = visu
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    egwindow.cpp \
    iostate.cpp \
    ogwindow.cpp \
    ugwindow.cpp \
    garagewindow.cpp \
    moduleservice.cpp \
    statusled.cpp \
    eventmonitor.cpp \
    kuechewindow.cpp \
    setupwindow.cpp \
    smartmeterwindow.cpp

HEADERS  += mainwindow.h \
    egwindow.h \
    iostate.h \
    ogwindow.h \
    ugwindow.h \
    garagewindow.h \
    moduleservice.h \
    statusled.h \
    eventmonitor.h \
    kuechewindow.h \
    setupwindow.h \
    smartmeterwindow.h

FORMS    += \
    mainwindow.ui \
    egwindow.ui \
    ogwindow.ui \
    ugwindow.ui \
    garagewindow.ui \
    kuechewindow.ui \
    setupwindow.ui \
    smartmeterwindow.ui

target.path += /root
INSTALLS += target
