include		FrgnRules.mk

ifneq "$(wildcard $(EPICS_INCLUDE_DIR)/cadef.h)" ""
  SUBDIRS=	cda
else
  DUMMY:=	$(warning WARNING: $(EPICS_INCLUDE_DIR)/cadef.h is absent, so skipping build of EPICS-related stuff)
endif

######################################################################
PRJDIR=		..
include		$(PRJDIR)/PrjRules.mk
######################################################################
