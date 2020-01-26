.PHONY:		firsttarget
firsttarget:	all

ifeq "$(VME_HAL_DESCR)" ""
  VME_HAL_DESCR=	$(VMEHAL_PFX)
endif

SHD_SYMLINKS=	$(VMEHAL_PFX)vme_lyr.c $(VMEHAL_PFX)_test.c

$(VMEHAL_PFX)vme_lyr.c:	$(SRCDIR)/vme_lyr_common.c
$(VMEHAL_PFX)_test.c:	$(SRCDIR)/vme_test_common.c

$(VMEHAL_PFX)vme_lyr.% $(VMEHAL_PFX)_test.%:	\
	SPECIFIC_DEFINES=-DVMEHAL_FILE_H='"$(VMEHAL_PFX)hal.h"' -DVMELYR_NAME=$(VMEHAL_PFX)vme -DVMEHAL_DESCR="$(VME_HAL_DESCR)"

$(SHD_SYMLINKS):
		$(SCRIPTSDIR)/ln-sf_safe.sh $< $@

SHD_GNTDFILES=	$(SHD_SYMLINKS)

SHD_INCLUDES=	-I$(SRCDIR)

# Declare ourself as "important"
MAKEFILE_PARTS+=$(SRCDIR)/ShadowRules.mk
