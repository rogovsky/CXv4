SRCDIR=		../src

LISTOFDRIVERS=	$(SRCDIR)/LISTOFCANDRIVERS.mk
MAKEFILE_PARTS+=$(LISTOFDRIVERS)
include		$(LISTOFDRIVERS)

DRIVERS=	$(addsuffix _drv.so, $(LISTOFCANDRIVERS))
MONO_C_FILES=	$(DRIVERS)

######################################################################
PRJDIR=		../../..
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

CANKOZ_DRV_SRCS=	$(addsuffix _drv.c, $(LISTOFCANDRIVERS))
CANKOZ_SYMLINKS=	$(CANKOZ_DRV_SRCS)
$(CANKOZ_SYMLINKS):	%: $(SRCDIR)/%

SYMLINKS=		$(CANKOZ_SYMLINKS)
LOCAL_GNTDFILES+=	$(SYMLINKS)

$(SYMLINKS):
			$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
