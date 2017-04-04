######################################################################

TEMPLATE = lib
CONFIG += qt warn_on exceptions release dll staticlib
QT -= gui
QMAKE_CFLAGS_DEBUG += -fPIC
QMAKE_CFLAGS_RELEASE += -fPIC

TARGET = Qt4cxscheduler
HEADERS += Qcxscheduler.h
SOURCES += Qt4cxscheduler.cpp

INCLUDEPATH += $${TOP_INCLUDES}
