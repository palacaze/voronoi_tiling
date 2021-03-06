QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = voronoi_tiling
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
        main.cpp \
        dialog.cpp

HEADERS += \
        dialog.h \
        poisson-grid.h

QMAKE_CFLAGS += -fno-omit-frame-pointer -g
QMAKE_CXXFLAGS += -fno-omit-frame-pointer -g
