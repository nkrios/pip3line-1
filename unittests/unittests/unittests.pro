# Released as open source by NCC Group Plc - http://www.nccgroup.com/
#
# Developed by Gabriel Caudrelier, gabriel dot caudrelier at nccgroup dot com
#
# https://github.com/nccgroup/pip3line
#
# Released under AGPL see LICENSE for more information

QT       += network svg xml xmlpatterns testlib

TARGET = tst_unitteststest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += tst_unitteststest.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

LIBS += -L"../../bin/" -ltransform
INCLUDEPATH += ../../libtransform
DESTDIR = ../../bin
