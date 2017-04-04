######################################################################
#                                                                    #
#  programs/utils/Makefile                                           #
#      Rules for making CX command-line utilities                    #
#                                                                    #
######################################################################

CDA_UTILS=		cdaclient das-experiment dataset-save
EXES=			drvinfo ppf4td_test \
			cxclient $(CDA_UTILS)

drvinfo_COMPONENTS=	drvinfo.o drvinfo_publics.o
ppf4td_test_COMPONENTS=	ppf4td_test.o
cxclient_COMPONENTS=	cxclient.o

cdaclient_COMPONENTS=		cdaclient.o        console_cda_util.o
das-experiment_COMPONENTS=	das-experiment.o   console_cda_util.o
dataset-save_COMPONENTS=	dataset-save.o     console_cda_util.o
dataset_restore_COMPONENTS=	dataset-restore.o  console_cda_util.o


SUBDIRS=	
TOPDIR=		../..
include $(TOPDIR)/TopRules.mk

drvinfo:		SPECIFIC_LIBS+=$(LIBDL)
drvinfo:		SPECIFIC_LDFLAGS=-Wl,-export-dynamic
drvinfo:		$(LIBCXSD) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)

ppf4td_test:		$(LIBUSEFUL) $(LIBMISC)

cxclient:		$(LIBCX_SYNC) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)

$(CDA_UTILS):		$(LIBCDA) $(LIBCX_ASYNC) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)
$(CDA_UTILS):		SPECIFIC_LIBS=$(LIBDL) -lm

EXPORTSFILES=		$(EXES)
EXPORTSDIR=		bin

# #### END OF Makefile ###############################################
