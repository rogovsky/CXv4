include		FrgnRules.mk

ifneq "$(wildcard $(TANGO_SERVER_INCLUDE_DIR)/tango.h)" ""
  SUBDIRS=	cda util
else
  DUMMY:=	$(warning WARNING: $(wildcard $(TANGO_SERVER_INCLUDE_DIR)/tango.h) is absent, so skipping build of TANGO-related stuff)
endif

######################################################################
PRJDIR=		..
include		$(PRJDIR)/PrjRules.mk
######################################################################
