######################################################################

TEMPLATE = lib
CONFIG += qt warn_on exceptions release dll staticlib
QT -= gui
QMAKE_CFLAGS_DEBUG += -fPIC
QMAKE_CFLAGS_RELEASE += -fPIC

TARGET = Qt5cxscheduler
HEADERS += Qt5cxscheduler.h
SOURCES += Qt5cxscheduler.cpp

INCLUDEPATH += $${TOP_INCLUDES}
