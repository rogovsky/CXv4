ARLIBS=			libcda_d_epics.a
SOLIBS=			cda_d_epics.so

libcda_d_epics.a_COMPONENTS=	cda_d_epics.o
cda_d_epics.so_COMPONENTS=	cda_d_epics.o

EXPORTSFILES=		$(SOLIBS)
EXPORTSDIR=		lib/cda/plugins

EXPORTSDIR2=		lib
EXPORTSFILES2=		$(ARLIBS)

SPECIFIC_CFLAGS=	-fPIC
DIR_INCLUDES=		$(EPICS_INCLUDES)
SPECIFIC_LDFLAGS=	-L$(EPICS_OS_LIB_DIR) -lca -lCom

######################################################################
PRJDIR=		../..
include		../FrgnRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################
