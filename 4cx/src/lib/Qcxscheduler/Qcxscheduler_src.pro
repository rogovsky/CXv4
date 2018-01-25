######################################################################

TEMPLATE = lib
CONFIG += qt warn_on exceptions release dll staticlib
QT -= gui
QMAKE_CFLAGS_DEBUG += -fPIC
QMAKE_CFLAGS_RELEASE += -fPIC

TARGET = Qt_ZZZ_VERSION_ZZZ_cxscheduler
HEADERS += Qt_ZZZ_VERSION_ZZZ_cxscheduler.h
SOURCES += Qt_ZZZ_VERSION_ZZZ_cxscheduler.cpp

INCLUDEPATH += $${TOP_INCLUDES}
