######################################################################
#                                                                    #
#  lib/cxlib/Makefile                                                #
#      Rules for making CX library                                   #
#                                                                    #
######################################################################

ARLIBS=		libcx_async.a libcx_sync.a
libcx_async.a_COMPONENTS=	cxlib_client.o cxlib_utils.o cxlib_async.o
libcx_sync.a_COMPONENTS=	cxlib_client.o cxlib_utils.o cxlib_sync.o

EXPORTSFILES=		$(ARLIBS)
EXPORTSDIR=		lib

SPECIFIC_CFLAGS=	-fPIC

TOPDIR=         ../..
include $(TOPDIR)/TopRules.mk

# #### END OF Makefile ###############################################
