######################################################################
#                                                                    #
#  lib/cda/Makefile                                                  #
#      Rules for making Cx Data Access library                       #
#                                                                    #
######################################################################

ifeq  "$(wildcard ../../../../cx/src/include/cx.h)" ""
  NOV2CX=YES
  DUMMY:=	$(warning WARNING: ../../../../cx/src/include/cx.h is absent, so skipping build of cda_d_v2cx.o)
else
  V2LIBS_DIR=		../v2cx
  v2all_COMPONENTS_file=$(V2LIBS_DIR)/v2all_COMPONENTS.mk
  include		$(v2all_COMPONENTS_file)
  MAKEFILE_PARTS+=	$(v2all_COMPONENTS_file)
endif

ARLIBS=		libcda.a libcda_d_insrv.a
libcda.a_COMPONENTS=	cda_core.o cda_err.o \
			cda_plugmgr.o \
			cda_d_cx.o cda_d_dircn.o cda_d_local.o \
			cda_d_vcas.o cda_d_epics.o \
			cda_f_fla.o
ifeq "$(NOV2CX)" ""
  libcda.a_COMPONENTS+=	cda_d_v2cx.o v2subsysaccess.o
endif
libcda.a_GIVEN_COMPONENTS=	$(foreach F, $(v2all_COMPONENTS), $(V2LIBS_DIR)/$F)
libcda_d_insrv.a_COMPONENTS=	cda_d_insrv.o

EXPORTSFILES=		$(ARLIBS)
EXPORTSDIR=		lib

SPECIFIC_CFLAGS=	-fPIC

TOPDIR=         ../..
include $(TOPDIR)/TopRules.mk

cda_d_v2cx.o   v2subsysaccess.o \
cda_d_v2cx.d   v2subsysaccess.d \
cda_d_v2cx.dep v2subsysaccess.dep: SPECIFIC_INCLUDES+=-I$(V2LIBS_DIR)
ifneq "$(NOV2CX)" ""
  cda_plugmgr.%:	SPECIFIC_DEFINES=-DNOV2CX
endif

# #### END OF Makefile ###############################################
