DRIVERS=	dl200me_drv.so adc200me_drv.so adc812me_drv.so
LAYERS=		pci4624_lyr.so
MONO_C_FILES=	$(DRIVERS) $(LAYERS)

######################################################################
PRJDIR=		../..
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

pci4624_lyr.%:	SPECIFIC_INCLUDES=-I$(PRJDIR)/../uspci/include

adc200me_drv.so adc812me_drv.so:	$(LIBPZFRAME_DRV)
