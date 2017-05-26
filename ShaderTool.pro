#-------------------------------------------------
#
# Project created by QtCreator 2015-12-28T12:16:09
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ShaderTool
TEMPLATE = app

CONFIG += c++14

LIBS += -lassimp -lm

SOURCES += main.cpp\
        mainwindow.cpp \
    shaderframe.cpp \
    mesh.cpp \
    glslhighlighter.cpp \
    shadereditor.cpp

HEADERS  += mainwindow.h \
    shaderframe.h \
    mesh.h \
    glslhighlighter.h \
    shadereditor.h

FORMS    += mainwindow.ui
