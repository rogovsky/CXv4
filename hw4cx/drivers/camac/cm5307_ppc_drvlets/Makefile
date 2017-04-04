

LOCAL_DEPENDS=	cm5307_DEFINE_DRIVER.d cm5307_camac.d
MONO_C_FILES=	cm5307_test
UNCFILES=	rrund

######################################################################
PRJDIR=		../../..

SECTION=	./DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

cm5307_test:	$(CM5307_CAMAC_O) $(LIBMISC)

rrund:		%:	$(X_RULES_DIR)/%
		cp -p $< $@
