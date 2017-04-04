######################################################################
#                                                                    #
#  lib/pzframe/Makefile                                              #
#      Rules for making pzframe-related libraries                    #
#                                                                    #
######################################################################

ARLIBS=		libpzframe_drv.a libpzframe_data.a
ifeq "$(NOX)" ""
  ARLIBS+=	libpzframe_gui.a
endif
include		libpzframe_drv.a_COMPONENTS.mk
include		libpzframe_data.a_COMPONENTS.mk
include		libpzframe_gui.a_COMPONENTS.mk

MAKEFILE_PARTS+=libpzframe_drv.a_COMPONENTS.mk \
		libpzframe_data.a_COMPONENTS.mk \
		libpzframe_gui.a_COMPONENTS.mk

EXPORTSFILES=	$(ARLIBS)
EXPORTSDIR=	lib

######################################################################
TOPDIR=         ../..
include $(TOPDIR)/TopRules.mk
######################################################################

$(libpzframe_drv.a_COMPONENTS):	SPECIFIC_CFLAGS=-fPIC

# #### END OF Makefile ###############################################
