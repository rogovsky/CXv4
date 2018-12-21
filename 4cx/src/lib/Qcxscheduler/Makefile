.PHONY:		firsttarget
firsttarget:	all
######################################################################
QT4LIB=		libQt4cxscheduler.a
QT4_PRO_FILE=	Qt4cxscheduler.pro
QT4_CPP_FILE=	Qt4cxscheduler.cpp
QT4_HDR_FILE=	Qt4cxscheduler.h
QT4_MAKEFILE=	Qt4cxscheduler.mk
CALL_QT4_MAKE=	make -f $(QT4_MAKEFILE)

QT5LIB=		libQt5cxscheduler.a
QT5_PRO_FILE=	Qt5cxscheduler.pro
QT5_CPP_FILE=	Qt5cxscheduler.cpp
QT5_HDR_FILE=	Qt5cxscheduler.h
QT5_MAKEFILE=	Qt5cxscheduler.mk
CALL_QT5_MAKE=	make -f $(QT5_MAKEFILE)

LOCAL_GNTDFILES=$(QT4_MAKEFILE) $(QT5_MAKEFILE) \
		$(QT4_CPP_FILE) $(QT5_CPP_FILE) \
		$(QT4_PRO_FILE) $(QT5_PRO_FILE) \
		$(QT4_HDR_FILE) $(QT5_HDR_FILE)

QTCHOOSER=	/usr/bin/qtchooser

ifneq "$(NOQT)" ""
  DUMMY:=$(warning WARNING: skipping build of Qcxscheduler because of NOQT switch)
else
  DUMMY:=$(warning WARNING: --------------------------------------------------------)
  DUMMY:=$(warning WARNING: This is an EXPERIMENTAL version of lib/Qcxscheduler/Makefile)
  DUMMY:=$(warning WARNING: which employs $(QTCHOOSER).)
  DUMMY:=$(warning WARNING: In case of problems or if Qcxscheduler is NOT needed,)
  DUMMY:=$(warning WARNING: just specify "NOQT=Y" in command line.)
  DUMMY:=$(warning WARNING: --------------------------------------------------------)

  ifneq "$(wildcard $(QTCHOOSER))" ""
    DUMMY := $(warning INFO: $(QTCHOOSER) found)
    QTS   := $(shell /usr/bin/qtchooser -l)
    DUMMY := $(warning INFO: Qt versions: $(QTS))
    ifneq "$(findstring qt5,$(QTS))" ""
      DUMMY := $(warning Qt5 found))
      QMAKE_QT5 = qtchooser -qt=qt5 -run-tool=qmake
    endif
    ifneq "$(findstring qt4,$(QTS))" ""
      DUMMY := $(warning Qt4 found))
      QMAKE_QT4 = qtchooser -qt=qt4 -run-tool=qmake
    endif
  else
    DUMMY:= $(warning WARNING: $(QTCHOOSER) is absent, trying to use qmakes)
    ifeq  "$(wildcard /usr/bin/qmake-qt4)" ""
      DUMMY:= $(warning WARNING: /usr/bin/qmake-qt4 is absent, so skipping build of libQt4cxscheduler)
    else
      QMAKE_QT4 = "qmake-qt4"
    endif
    ifeq  "$(wildcard /usr/bin/qmake-qt5)" ""
      DUMMY:= $(warning WARNING: /usr/bin/qmake-qt5 is absent, so skipping build of libQt5cxscheduler)
    else
      QMAKE_QT5 = "qmake-qt5"
    endif
  endif
endif

ifneq "$(QMAKE_QT4)" ""
  UNCFILES+=		$(QT4LIB)
  MAKEFILE_PARTS+=	$(QT4_PRO_FILE)

  $(QT4_MAKEFILE):	$(QT4_PRO_FILE) $(QT4_CPP_FILE) $(QT4_HDR_FILE)
		$(QMAKE_QT4) -o $@ $< TOP_INCLUDES=$(TOPDIR)/include

  libQt4cxscheduler.a:	$(QT4_MAKEFILE) $(QT4_CPP_FILE) $(QT4_HDR_FILE)
		$(CALL_QT4_MAKE)

  Qt4-clean:	$(QT4_MAKEFILE)
		$(CALL_QT4_MAKE) clean
  Qt4-distclean:$(QT4_MAKEFILE)
		$(CALL_QT4_MAKE) distclean
endif

ifneq "$(QMAKE_QT5)" ""
  UNCFILES+=		$(QT5LIB)
  MAKEFILE_PARTS+=	$(QT5_PRO_FILE)

  $(QT5_MAKEFILE):	$(QT5_PRO_FILE) $(QT5_CPP_FILE) $(QT5_HDR_FILE)
		$(QMAKE_QT5) -o $@ $< TOP_INCLUDES=$(TOPDIR)/include

  libQt5cxscheduler.a:	$(QT5_MAKEFILE) $(QT5_CPP_FILE) $(QT5_HDR_FILE)
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

$(QT4_PRO_FILE) $(QT5_PRO_FILE): Qt%cxscheduler.pro: Qcxscheduler_src.pro
		sed -e 's/_ZZZ_VERSION_ZZZ_/$*/g' <$< >$@

$(QT4_CPP_FILE) $(QT5_CPP_FILE): Qt%cxscheduler.cpp: Qcxscheduler_src.cpp
		sed -e 's/_ZZZ_VERSION_ZZZ_/$*/g' <$< >$@

$(QT4_HDR_FILE) $(QT5_HDR_FILE): Qt%cxscheduler.h:   Qcxscheduler_src.h
		$(TOPDIR)/scripts/ln-sf_safe.sh    $<  $@

# --------------------------------------------------------------------
