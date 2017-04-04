.PHONY:		firsttarget
firsttarget:	all
######################################################################
QT4LIB=		libQt4cxscheduler.a
QT4_PRO_FILE=	Qt4cxscheduler.pro
QT4_CPP_FILE=	Qt4cxscheduler.cpp
QT4_MAKEFILE=	Qt4cxscheduler.mk
CALL_QT4_MAKE=	make -f $(QT4_MAKEFILE)

QT5LIB=		libQt5cxscheduler.a
QT5_PRO_FILE=	Qt5cxscheduler.pro
QT5_CPP_FILE=	Qt5cxscheduler.cpp
QT5_MAKEFILE=	Qt5cxscheduler.mk
CALL_QT5_MAKE=	make -f $(QT5_MAKEFILE)

LOCAL_GNTDFILES=$(QT4_MAKEFILE) $(QT5_MAKEFILE) $(QT4_CPP_FILE) $(QT5_CPP_FILE)

ifeq  "$(wildcard /usr/bin/qmake-qt4)" ""
  DUMMY:=	$(warning WARNING: /usr/bin/qmake-qt4 is absent, so skipping build of libQt4cxscheduler)
else
  UNCFILES+=		$(QT4LIB)
  MAKEFILE_PARTS+=	$(QT4_PRO_FILE)

  $(QT4_MAKEFILE):	$(QT4_PRO_FILE) $(QT4_CPP_FILE) # CPP only to prevent qmake's warning
		qmake-qt4 -o $@ $< TOP_INCLUDES=$(TOPDIR)/include

  libQt4cxscheduler.a:	$(QT4_MAKEFILE) $(QT4_CPP_FILE) Qcxscheduler.h
		$(CALL_QT4_MAKE)

  Qt4-clean:	$(QT4_MAKEFILE)
		$(CALL_QT4_MAKE) clean
  Qt4-distclean:$(QT4_MAKEFILE)
		$(CALL_QT4_MAKE) distclean
endif

ifeq  "$(wildcard /usr/bin/qmake-qt5)" ""
  DUMMY:=	$(warning WARNING: /usr/bin/qmake-qt5 is absent, so skipping build of libQt5cxscheduler)
else
  UNCFILES+=		$(QT5LIB)
  MAKEFILE_PARTS+=	$(QT5_PRO_FILE)

  $(QT5_MAKEFILE):	$(QT5_PRO_FILE) $(QT5_CPP_FILE) # CPP only to prevent qmake's warning
		qmake-qt5 -o $@ $< TOP_INCLUDES=$(TOPDIR)/include

  libQt5cxscheduler.a:	$(QT5_MAKEFILE) $(QT5_CPP_FILE) Qcxscheduler.h
		$(CALL_QT5_MAKE)

  Qt5-clean:	$(QT5_MAKEFILE)
		$(CALL_QT5_MAKE) clean
  Qt5-distclean:$(QT5_MAKEFILE)
		$(CALL_QT5_MAKE) distclean
endif

EXPORTSFILES=	$(UNCFILES)
EXPORTSDIR=	lib

######################################################################
TOPDIR=	        ../..
include		$(TOPDIR)/TopRules.mk
######################################################################

.PHONY:		local-clean   local-distclean \
		Qt4-clean     Qt4-distclean \
		Qt5-clean     Qt5-distclean
local-clean:	Qt4-clean     Qt5-clean
local-distclean:Qt4-distclean Qt5-distclean

clean:		local-clean
distclean maintainer-clean crit-clean:	local-distclean

$(QT4_CPP_FILE) $(QT5_CPP_FILE): Qcxscheduler.cpp
		$(SCRIPTSDIR)/ln-sf_safe.sh $< $@

# --------------------------------------------------------------------
