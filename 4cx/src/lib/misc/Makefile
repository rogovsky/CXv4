######################################################################
#                                                                    #
#  lib/misc/Makefile                                                 #
#      Rules for making "misc. utils" library                        #
#                                                                    #
######################################################################

ARLIBS=		libmisc.a
include		libmisc.a_COMPONENTS.mk

MAKEFILE_PARTS+=libmisc.a_COMPONENTS.mk

EXPORTSFILES=	$(ARLIBS)
EXPORTSDIR=	lib

SPECIFIC_CFLAGS=	-fPIC

TOPDIR=		../..
include $(TOPDIR)/TopRules.mk

# #### END OF Makefile ###############################################
