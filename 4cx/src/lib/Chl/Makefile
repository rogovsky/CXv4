######################################################################
#                                                                    #
#  lib/Chl/Makefile                                                  #
#      Rules for making Cx High-Level library                        #
#                                                                    #
######################################################################

ARLIBS=		libChl.a
libChl.a_COMPONENTS=	Chl_privrec.o Chl_err.o \
			Chl_app.o \
			Chl_knobprops.o Chl_bigvals.o Chl_histplot.o \
			Chl_help.o

EXPORTSFILES=	$(ARLIBS)
EXPORTSDIR=	lib

TOPDIR=         ../..
include $(TOPDIR)/TopRules.mk

# #### END OF Makefile ###############################################
