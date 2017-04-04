######################################################################
#                                                                    #
#  programs/tests/Makefile                                           #
#      Rules for making CX test applications                         #
#                                                                    #
######################################################################

CHL_EXES=	dircntest localtest
EXES=		tstknobs $(CHL_EXES) cdadbltest
tstknobs_COMPONENTS=	tstknobs.o
dircntest_COMPONENTS=	dircntest.o
localtest_COMPONENTS=	localtest.o
cdadbltest_COMPONENTS=	cdadbltest.o

SUBDIRS=
TOPDIR=		../..
include $(TOPDIR)/TopRules.mk


# Damn, a hack for FC4@x86_64, where libXm.so is found in /usr/X11R6/lib64
ifeq "$(CPU)" "X86_64"
  MOTIF_LIBS_DIR=/usr/X11R6/lib64
else
  MOTIF_LIBS_DIR=/usr/X11R6/lib
endif
tstknobs:	SPECIFIC_LIBS=-L$(MOTIF_LIBS_DIR) \
		-lm -lXm -lXpm -lXt -lSM -lICE -lXext -lXmu -lX11
tstknobs:       $(LIBKNOBSCORE) $(LIBMOTIFKNOBS) \
		$(LIBCDR) $(LIBDATATREE) \
		$(LIBCDA) $(LIBCX_ASYNC) \
		$(LIBXH) $(LIBAUXMOTIFWIDGETS) \
		$(LIBUSEFUL) $(LIBXH_CXSCHEDULER) $(LIBMISC)
$(CHL_EXES):	SPECIFIC_LIBS=-L$(MOTIF_LIBS_DIR) \
		-lm -lXm -lXpm -lXt -lSM -lICE -lXext -lXmu -lX11
$(CHL_EXES):	$(LIBCHL) $(LIBKNOBSCORE) $(LIBMOTIFKNOBS) $(LIBMOTIFKNOBS_CDA) \
		$(LIBCDR) $(LIBDATATREE) \
		$(LIBCDA) $(LIBCX_ASYNC) \
		$(LIBXH) $(LIBAUXMOTIFWIDGETS) \
		$(LIBUSEFUL) $(LIBXH_CXSCHEDULER) $(LIBMISC)
cdadbltest:	$(LIBCDA) $(LIBCX_ASYNC) \
		$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)
cdadbltest:	SPECIFIC_LIBS+=-lm

LOCAL_LIBS+=	$(LIBDL)

# #### END OF Makefile ###############################################
