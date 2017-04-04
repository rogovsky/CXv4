SRCDIR=		../src
CANHAL_PFX=	xrt
include		$(SRCDIR)/ShadowRules.mk
#

MONO_C_FILES=	$(CANHAL_PFX)cankoz_lyr.so $(CANHAL_PFX)canmon

######################################################################
PRJDIR=		../../..
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

xrtcanmon:	$(LIBMISC)
