######################################################################
#                                                                    #
#  programs/server/Makefile                                          #
#      Rules for making CX server                                    #
#                                                                    #
######################################################################

EXES=		cxsd
cxsd_COMPONENTS=	cxsd.o cxsd_vars.o cxsd_config.o \
			cxsd_publics.o \
			cxsd_builtins.o

SUBDIRS=
TOPDIR=		../..
include $(TOPDIR)/TopRules.mk

LOCAL_WORKFILES=	*.log *.pid

cxsd:		SPECIFIC_LDFLAGS=-Wl,-export-dynamic -lm
cxsd:		SPECIFIC_LIBS+=$(LIBDL)
cxsd:		$(LIBCDA_D_INSRV) $(LIBCDA) $(LIBCX_ASYNC) \
		$(LIBCXSD_PLUGINS) $(LIBCXSD) \
		$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)

EXPORTSFILES=	$(EXES)
EXPORTSDIR=	sbin

EXPORTSFILES2=	cxsd.conf
EXPORTSDIR2=	configs

# ----

LOCAL_GNTDFILES=cxsd_builtins.c

# #### END OF Makefile ###############################################
