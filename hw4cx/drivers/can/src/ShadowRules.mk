.PHONY:		firsttarget
firsttarget:	all

ifeq "$(CANKOZ_HAL_DESCR)" ""
  CANKOZ_HAL_DESCR=	$(CANHAL_PFX)
endif

SHD_SYMLINKS=	$(CANHAL_PFX)cankoz_lyr.c $(CANHAL_PFX)canmon.c

$(CANHAL_PFX)cankoz_lyr.c:	$(SRCDIR)/cankoz_lyr_common.c
$(CANHAL_PFX)canmon.c:		$(SRCDIR)/canmon_common.c

$(CANHAL_PFX)cankoz_lyr.% $(CANHAL_PFX)canmon.%:	\
	SPECIFIC_DEFINES=-DCANHAL_FILE_H='"$(CANHAL_PFX)canhal.h"' -DCANLYR_NAME=$(CANHAL_PFX)cankoz -DCANHAL_DESCR="$(CANKOZ_HAL_DESCR)"

$(SHD_SYMLINKS):
		$(SCRIPTSDIR)/ln-sf_safe.sh $< $@

SHD_GNTDFILES=	$(SHD_SYMLINKS)

SHD_INCLUDES=	-I$(SRCDIR)

# Declare ourself as "important"
MAKEFILE_PARTS+=$(SRCDIR)/ShadowRules.mk
