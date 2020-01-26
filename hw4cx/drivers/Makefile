######################################################################
PRJDIR=		..
######################################################################

SUBDIRS=        can serial eth camac vme

# Value derived from cpci/Makefile
USPCI_CANARY=$(PRJDIR)/../uspci/include/uspci.h
ifeq "$(wildcard $(USPCI_CANARY))" ""
  DUMMY:=	$(warning WARNING: skipping build of cPCI drivers because of missing $(USPCI_CANARY))
else
  SUBDIRS+=	cpci
endif

DAQMX_SUBDIR=	daqmx
include		$(DAQMX_SUBDIR)/DAQmxDefs.mk
ifneq ":$(MAY_BUILD_DAQMX_DRIVERS):" ":YES:"
  DUMMY:=	$(warning WARNING: skipping build of DAQmx-related stuff because of missing $(DAQMX_INCLUDE_DIR)/NIDAQmx.h)
else
  SUBDIRS+=	$(DAQMX_SUBDIR)
endif

######################################################################
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################
