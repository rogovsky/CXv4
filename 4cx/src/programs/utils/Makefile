######################################################################
#                                                                    #
#  programs/utils/Makefile                                           #
#      Rules for making CX command-line utilities                    #
#                                                                    #
######################################################################

CDA_UTILS=		cdaclient das-experiment dataset-save pipe2cda
EXES=			drvinfo ppf4td_test \
			cxclient cx-chan-search $(CDA_UTILS)

drvinfo_COMPONENTS=	drvinfo.o drvinfo_publics.o
ppf4td_test_COMPONENTS=	ppf4td_test.o
cxclient_COMPONENTS=	cxclient.o
cx-chan-search_COMPONENTS=	cx-chan-search.o

cdaclient_COMPONENTS=		cdaclient.o        console_cda_util.o
das-experiment_COMPONENTS=	das-experiment.o   console_cda_util.o
dataset-save_COMPONENTS=	dataset-save.o     console_cda_util.o
dataset_restore_COMPONENTS=	dataset-restore.o  console_cda_util.o
pipe2cda_COMPONENTS=		pipe2cda.o         console_cda_util.o


SUBDIRS=	
TOPDIR=		../..
include $(TOPDIR)/TopRules.mk

drvinfo:		SPECIFIC_LIBS+=$(LIBDL) -lm # "-lm" is because of round() in cxsd_hw.c, added 22.05.2019
drvinfo:		SPECIFIC_LDFLAGS=-Wl,-export-dynamic
drvinfo:		$(LIBCXSD) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)

ppf4td_test:		$(LIBUSEFUL) $(LIBMISC)

cxclient:		$(LIBCX_SYNC) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)

cx-chan-search:		$(LIBCX_SYNC) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)

$(CDA_UTILS):		$(LIBCDA) $(LIBCX_ASYNC) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)
$(CDA_UTILS):		SPECIFIC_LIBS=$(LIBDL) -lm
$(CDA_UTILS):		SPECIFIC_LDFLAGS=-Wl,-export-dynamic # for dynamic loading of dat-plugins

EXPORTSFILES=		$(EXES)
EXPORTSDIR=		bin

# #### END OF Makefile ###############################################
