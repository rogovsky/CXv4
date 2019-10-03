######################################################################
#                                                                    #
#  programs/tests/Makefile                                           #
#      Rules for making CX test applications                         #
#                                                                    #
######################################################################

CHL_EXES=	pult stand
EXES=		$(CHL_EXES) histplot
pult_COMPONENTS=	pult.o
stand_COMPONENTS=	stand.o
histplot_COMPONENTS=	histplot.o

SUBDIRS=
TOPDIR=		../..
include $(TOPDIR)/TopRules.mk
# Damn, a hack for FC4@x86_64, where libXm.so is found in /usr/X11R6/lib64
ifeq "$(CPU)" "X86_64"
  MOTIF_LIBS_DIR=/usr/X11R6/lib64
else
  MOTIF_LIBS_DIR=/usr/X11R6/lib
endif

stand:		$(LIBCDA_D_INSRV) $(LIBCXSD_PLUGINS) $(LIBCXSD) # Note: this line should be BEFORE common "$(CHL_EXES): ...", because order of libraries does matter (here specified libs depend on libuseful+libmisc)

DIR_LIBS=	-L$(MOTIF_LIBS_DIR) \
		-lm -lXm -lXpm -lXt -lSM -lICE -lXext -lXmu -lX11 \
		$(LIBDL)
DIR_LDFLAGS=	-Wl,-export-dynamic # for dynamic loading of dat-plugins
$(CHL_EXES):	$(LIBCHL) \
		$(LIBKNOBSCORE) $(LIBMOTIFKNOBS) $(LIBMOTIFKNOBS_CDA) \
		$(LIBCDR) $(LIBDATATREE) \
		$(LIBCDA) $(LIBCX_ASYNC) \
		$(LIBXH)  $(LIBAUXMOTIFWIDGETS) \
		$(LIBUSEFUL) $(LIBXH_CXSCHEDULER) $(LIBMISC)

histplot:	$(LIBKNOBSCORE) $(LIBMOTIFKNOBS) $(LIBMOTIFKNOBS_CDA) \
		$(LIBCDR) $(LIBDATATREE) \
		$(LIBCDA) $(LIBCX_ASYNC) \
		$(LIBXH)  $(LIBAUXMOTIFWIDGETS) \
		$(LIBUSEFUL) $(LIBXH_CXSCHEDULER) $(LIBMISC)

EXPORTSFILES=	$(EXES)
EXPORTSDIR=	bin

# #### END OF Makefile ###############################################
