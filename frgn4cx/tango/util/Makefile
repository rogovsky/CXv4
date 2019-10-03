FRGN=				tango

CDA_UTILS=			cdaclient das-experiment dataset-save

FRGN_CDA_UTILS=			$(foreach U, $(CDA_UTILS), $(FRGN)_$U)
EXES=				$(FRGN_CDA_UTILS)

FRGN_LIBS=			../cda/libcda_d_tango.a

$(FRGN)_cdaclient_COMPONENTS=	  $(FRGN)_cdaclient.o      console_cda_util.o $(FRGN)_builtins.o
$(FRGN)_das-experiment_COMPONENTS=$(FRGN)_das-experiment.o console_cda_util.o $(FRGN)_builtins.o
$(FRGN)_dataset-save_COMPONENTS=  $(FRGN)_dataset-save.o   console_cda_util.o $(FRGN)_builtins.o

######################################################################
PRJDIR=		../..
include		../FrgnRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

SUBJ_DIR=	$(TOPDIR)/programs/utils

LOCAL_INCLUDES=	-I$(SUBJ_DIR)

console_cda_util.c:	%:	$(TOPDIR)/programs/utils/%
$(FRGN_CDA_UTILS:=.c):	$(FRGN)_%.c:	$(SUBJ_DIR)/%.c
$(FRGN_CDA_UTILS:=%): \
	SPECIFIC_DEFINES=-DBUILTINS_DECLARATION_H_FILE='"main_builtins.h"' \
			 -DBUILTINS_REGISTRATION_CODE='InitBuiltins(NULL);'

$(FRGN_CDA_UTILS):	$(LIBCDA) $(LIBCX_ASYNC) \
			$(FRGN_LIBS) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)
$(FRGN_CDA_UTILS):	SPECIFIC_LIBS=$(LIBDL) $(TANGO_LIBS) -lm

SYMLINKS=		console_cda_util.c $(FRGN_CDA_UTILS:=.c)
LOCAL_GNTDFILES+=	$(SYMLINKS) tango_builtins.c

$(SYMLINKS):
	$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
