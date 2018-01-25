

LOCAL_DEPENDS=	bivme2_DEFINE_DRIVER.d bivme2_io.d
MONO_C_FILES=	bivme2_test
UNCFILES=	rrund

######################################################################
PRJDIR=		../../..

SECTION=	./DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

bivme2_test:	$(BIVME2_IO_O) $(LIBVMEDIRECT) $(LIBMISC)

rrund:		%:	$(X_RULES_DIR)/%
		cp -p $< $@
