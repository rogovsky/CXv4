######################################################################
#                                                                    #
#  programs/runner/Makefile                                          #
#      Rules for making CX-starter and siblings                      #
#                                                                    #
######################################################################

UNCFILES=	cstart
EXES=		cx-starter
cx-starter_COMPONENTS=	cx-starter.o \
			cx-starter_msg.o \
			cx-starter_cfg.o cx-starter_con.o \
			cx-starter_Cdr.o \
			cx-starter_x11.o cx-starter_gui.o

SUBDIRS=
TOPDIR=		../..
include $(TOPDIR)/TopRules.mk

DIR_LIBS=	-lm $(MOTIF_LIBS)
cx-starter:	$(LIBMOTIFKNOBS_CDA) \
		$(LIBCDR) $(LIBDATATREE) \
		$(LIBCDA) $(LIBCX_ASYNC) \
		$(LIBXH)  $(LIBAUXMOTIFWIDGETS) \
		$(LIBUSEFUL) $(LIBXH_CXSCHEDULER) $(LIBMISC) \
		$(LIBDL)

cstart:		cstart.sh
		cp -p $< $@  &&  chmod +x $@

EXPORTSFILES=	$(EXES) cstart
EXPORTSDIR=	bin

# #### END OF Makefile ###############################################
