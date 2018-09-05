######################################################################
#                                                                    #
#  lib/srv/Makefile                                                  #
#      Rules for making CXSD library and standard plugins            #
#                                                                    #
######################################################################

ARLIBS=		libcxsd.a libcxsd_plugins.a
libcxsd.a_COMPONENTS=	cxsd_logger.o \
			cxsd_modmgr.o cxsd_driver.o \
			cxsd_db.o cxsd_db_plugmgr.o \
			cxsd_hw.o
libcxsd_plugins.a_COMPONENTS=	cxsd_db_via_ppf4td.o \
			cxsd_db_file_ldr.o \
			cxsd_fe_cx.o

#SOLIBS=		        cxsd_fe_cx.so cxsd_db_file_ldr.so
#cxsd_fe_cx.so_COMPONENTS=	cxsd_fe_cx.o
#cxsd_db_file_ldr.so_COMPONENTS=	cxsd_db_file_ldr.o cxsd_db_via_ppf4td.o

SOLIBS=			cxsd_fe_starogate.so
cxsd_fe_starogate.so_COMPONENTS=cxsd_fe_starogate.o
$(cxsd_fe_starogate.so_COMPONENTS):	SPECIFIC_CFLAGS=-fPIC

EXPORTSFILES=	$(ARLIBS)
EXPORTSDIR=	lib

TOPDIR=         ../..
include $(TOPDIR)/TopRules.mk

$(libcxsd_plugins.a_COMPONENTS):	SPECIFIC_CFLAGS=-fPIC

# #### END OF Makefile ###############################################
