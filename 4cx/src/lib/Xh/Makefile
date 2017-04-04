######################################################################
#                                                                    #
#  lib/Xh/Makefile                                                   #
#      Rules for making Xh (X11 High-level) library                  #
#                                                                    #
######################################################################

SOLIBS=				bigredcur.so
bigredcur.so_COMPONENTS=	bigredcur.o

ARLIBS=				libXh.a libXh_cxscheduler.a
libXh.a_COMPONENTS=		Xh_app.o Xh_globals.o Xh_utils.o Xh_colors.o \
				Xh_grid.o Xh_balloon.o Xh_hilite.o \
				Xh_window.o Xh_toolbar.o Xh_statusline.o \
				Xh_viewport.o Xh_plot.o Xh_monoimg.o \
				Xh_fdlg.o Xh_stddlg.o
libXh_cxscheduler.a_COMPONENTS=	Xh_cxscheduler.o

EXPORTSFILES=			$(ARLIBS) $(SOLIBS)
EXPORTSDIR=			lib

TOPDIR=		../..
include $(TOPDIR)/TopRules.mk

bigredcur.so:	SPECIFIC_LIBS=$(LIBDL)
bigredcur.o:	SPECIFIC_CFLAGS=-fPIC

# #### END OF Makefile ###############################################
