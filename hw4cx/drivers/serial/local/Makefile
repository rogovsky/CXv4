SRCDIR=		../src

LISTOFDRIVERS=	$(SRCDIR)/LISTOFSERIALDRIVERS.mk
MAKEFILE_PARTS+=$(LISTOFDRIVERS)
include		$(LISTOFDRIVERS)

DRIVERS=	$(addsuffix _drv.so, $(LISTOFSERIALDRIVERS))
MONO_C_FILES=	$(DRIVERS)

######################################################################
PRJDIR=		../../..
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

SERIAL_DRV_SRCS=	$(addsuffix _drv.c, $(LISTOFSERIALDRIVERS))
SERIAL_SYMLINKS=	$(SERIAL_DRV_SRCS)
$(SERIAL_SYMLINKS):	%: $(SRCDIR)/%

SYMLINKS=		$(SERIAL_SYMLINKS)
LOCAL_GNTDFILES+=	$(SYMLINKS)

$(SYMLINKS):
			$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
