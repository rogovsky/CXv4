SRCDIR=		../src
SERIAL_TYPE=	com
SERIALDEV_PFX=	S
include		$(SRCDIR)/ShadowRules.mk

MONO_C_FILES=   $(SERIAL_TYPE)piv485_lyr.so $(UTILSSOURCES:.c=)

######################################################################
PRJDIR=		../../..
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

$(SERIAL_TYPE)setupkshd485 $(SERIAL_TYPE)dumpserial:	$(LIBMISC)
