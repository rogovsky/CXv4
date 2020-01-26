SRCDIR=		../nsrc
VMEHAL_PFX=	bivme2
include		$(SRCDIR)/ShadowRules.mk

#--- VmeserverShadowRules.mk -----------------------------------------
# A copy of CanserverShadowRules.mk, with "CAN" replaced by "VME"

LISTOFDRIVERS=	$(SRCDIR)/LISTOFVMEDRIVERS.mk
MAKEFILE_PARTS+=$(LISTOFDRIVERS)
include		$(LISTOFDRIVERS)

LISTOFVMELAYERS=$(VMEHAL_PFX)vme
VME_LYR_SRCS=	$(addsuffix _lyr.c, $(LISTOFVMELAYERS))

VME_DRV_SRCS=		$(addsuffix _drv.c, $(LISTOFVMEDRIVERS))
VME_SYMLINKS=		$(VME_DRV_SRCS)
$(VME_SYMLINKS):	%: $(SRCDIR)/%
			$(SCRIPTSDIR)/ln-sf_safe.sh $< $@

v4$(VMEHAL_PFX)vmeserver.c:	$(SRCDIR)/vmeserver_common.c
			$(SCRIPTSDIR)/ln-sf_safe.sh $< $@

vmeserver_drivers.c:	$(LISTOFDRIVERS) Makefile
		(                                                      \
		  echo \#include'"remsrv.h"';                          \
		  echo;                                                \
		  $(foreach N, $(LISTOFVMEDRIVERS),                    \
		    echo 'extern CxsdDriverModRec $N_driver_modrec;';) \
		  echo 'static CxsdDriverModRec * drvtable[] = {';     \
		  $(foreach N, $(LISTOFVMEDRIVERS),                    \
		    echo '    &$N_driver_modrec,';)                    \
		  echo '    NULL';                                     \
		  echo '};';                                           \
		  echo 'CxsdDriverModRec **remsrv_drivers = drvtable;';\
		  echo;                                                \
		  $(foreach N, $(LISTOFVMELAYERS),                     \
		    echo 'extern CxsdLayerModRec $N_layer_modrec;';)   \
		  echo 'static CxsdLayerModRec * lyrtable[] = {';      \
		  echo   '    NULL,';                                  \
		  $(foreach N, $(LISTOFVMELAYERS),                     \
		    echo '    &$N_layer_modrec,';)                     \
		  echo '};';                                           \
		  echo 'CxsdLayerModRec   **remcxsd_layers = lyrtable;';\
		  echo 'int remcxsd_numlyrs = countof(lyrtable);';     \
		) >$@

v4$(VMEHAL_PFX)vmeserver_COMPONENTS=	v4$(VMEHAL_PFX)vmeserver.o vmeserver_drivers.o \
				$(VME_LYR_SRCS:.c=.o) \
				$(VME_DRV_SRCS:.c=.o)

LOCAL_GNTDFILES+=v4$(VMEHAL_PFX)vmeserver.c vmeserver_drivers.c $(VME_DRV_SRCS)
#---------------------------------------------------------------------

MONO_C_FILES=	$(VMEHAL_PFX)_test
EXES=		v4$(VMEHAL_PFX)vmeserver

EXPORTSFILES=	$(EXES) $(MONO_C_FILES)
EXPORTSDIR=	lib/server/drivers/bivme2_drvlets

#---------------------------------------------------------------------
MONO_C_FILES+=	zzz_drv.so

######################################################################
PRJDIR=         ../../..
X_RULES_DIR=	$(PRJDIR)/x-build/ppc860-linux

SECTION=	$(X_RULES_DIR)/x_rules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

GIVEN_DIR=	$(PRJDIR)/given/bivme2
LOCAL_INCLUDES+=-I$(GIVEN_DIR)

LIBVMEDIRECT=	$(GIVEN_DIR)/libvmedirect.a

v4$(VMEHAL_PFX)vmeserver.%:	SPECIFIC_DEFINES=-DVMESERVER_ISSUE='"CXv4 BIVME2 vmeserver"'
v4$(VMEHAL_PFX)vmeserver:       $(LIBVMEDIRECT) \
				$(LIBREMSRV) \
				$(LIBPZFRAME_DRV) \
				$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)
v4$(VMEHAL_PFX)vmeserver:	SPECIFIC_LIBS+=$(LIBDL)

#???
$(VMEHAL_PFX)_test:	$(LIBVMEDIRECT) $(LIBMISC)
