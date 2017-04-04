SRCDIR=		../src
CANHAL_PFX=	mar
include		$(SRCDIR)/ShadowRules.mk
#

MONO_C_FILES=	$(CANHAL_PFX)cankoz_lyr.so $(CANHAL_PFX)canmon

######################################################################
PRJDIR=		../../..
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################
GIVEN_DIR=	$(PRJDIR)/given/marathon-2.4
LOCAL_INCLUDES+=-I$(GIVEN_DIR)

marcanmon:	$(LIBMISC)
