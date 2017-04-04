.PHONY:		firsttarget
firsttarget:	all

LISTOFDRIVERS=	$(SRCDIR)/ListOfCamacDrivers.mk
MAKEFILE_PARTS+=$(LISTOFDRIVERS)
include		$(LISTOFDRIVERS)

DIRECTDRIVERSSOURCES=	$(addprefix $(CAMAC_PFX)_, \
			  $(addsuffix _drv.c, $(CAMACDRIVERS)) \
			 )
CAMACDRIVERSSYMLINKS=	$(DIRECTDRIVERSSOURCES)
$(CAMACDRIVERSSYMLINKS):	$(CAMAC_PFX)_%:	$(SRCDIR)/%

# General symlinks' management
SHD_SYMLINKS=	$(CAMACDRIVERSSYMLINKS)
SHD_GNTDFILES=	$(SHD_SYMLINKS)

$(SHD_SYMLINKS):
		$(SCRIPTSDIR)/ln-sf_safe.sh $< $@

SHD_INCLUDES=	-I$(SRCDIR)
