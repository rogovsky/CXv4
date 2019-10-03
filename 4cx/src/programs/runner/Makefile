######################################################################
#                                                                    #
#  programs/runner/Makefile                                          #
#      Rules for making CX-starter and siblings                      #
#                                                                    #
######################################################################

SH_UTILS=	cstart run-cx-server start-all-servers stop-all-servers
UNCFILES=	$(SH_UTILS)
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
cx-starter:	SPECIFIC_LIBS=$(LIBDL)
cx-starter:	SPECIFIC_LDFLAGS=-Wl,-export-dynamic # for dynamic loading of dat-plugins
cx-starter:	$(LIBMOTIFKNOBS_CDA) \
		$(LIBCDR) $(LIBDATATREE) \
		$(LIBCDA) $(LIBCX_ASYNC) \
		$(LIBXH)  $(LIBAUXMOTIFWIDGETS) \
		$(LIBUSEFUL) $(LIBXH_CXSCHEDULER) $(LIBMISC)

$(SH_UTILS):	%: %.sh
		cp -p $< $@  &&  chmod +x $@

EXPORTSFILES=	$(EXES) $(SH_UTILS)
EXPORTSDIR=	bin

# #### END OF Makefile ###############################################
