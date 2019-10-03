SUBDIRS=        can cpci serial eth camac vme

DAQMX_SUBDIR=	daqmx
include		$(DAQMX_SUBDIR)/DAQmxDefs.mk
ifeq ":$(MAY_BUILD_DAQMX_DRIVERS):" ":YES:"
  SUBDIRS+=	$(DAQMX_SUBDIR)
else
  DUMMY:=	$(warning WARNING: $(DAQMX_INCLUDE_DIR)/NIDAQmx.h is absent, so skipping build of DAQmx-related stuff)
endif

######################################################################
PRJDIR=		..
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################
