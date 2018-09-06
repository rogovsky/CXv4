# CAN
SRCDIR=		../src
CANHAL_PFX=	skt
include		$(SRCDIR)/ShadowRules.mk

#--- CanserverShadowRules.mk -----------------------------------------
LISTOFDRIVERS=	$(SRCDIR)/LISTOFCANDRIVERS.mk
MAKEFILE_PARTS+=$(LISTOFDRIVERS)
include		$(LISTOFDRIVERS)
#
LISTOFCANLAYERS=$(CANHAL_PFX)cankoz
CANKOZ_LYR_SRCS=$(addsuffix _lyr.c, $(LISTOFCANLAYERS))
#
CANKOZ_DRV_SRCS=	$(addsuffix _drv.c, $(LISTOFCANDRIVERS))
CANKOZ_SYMLINKS=	$(CANKOZ_DRV_SRCS)
$(CANKOZ_SYMLINKS):	%: $(SRCDIR)/%
			$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
#
v4$(CANHAL_PFX)canserver.c:		$(SRCDIR)/canserver_common.c
			$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
#
canserver_drivers.c:	$(LISTOFDRIVERS) Makefile
		(                                                      \
		  echo \#include'"remsrv.h"';                          \
		  echo;                                                \
		  $(foreach N, $(LISTOFCANDRIVERS),                    \
		    echo 'extern CxsdDriverModRec $N_driver_modrec;';) \
		  echo 'static CxsdDriverModRec * drvtable[] = {';     \
		  $(foreach N, $(LISTOFCANDRIVERS),                    \
		    echo '    &$N_driver_modrec,';)                    \
		  echo '    NULL';                                     \
		  echo '};';                                           \
		  echo 'CxsdDriverModRec **remsrv_drivers = drvtable;';\
		  echo;                                                \
		  $(foreach N, $(LISTOFCANLAYERS),                     \
		    echo 'extern CxsdLayerModRec $N_layer_modrec;';)   \
		  echo 'static CxsdLayerModRec * lyrtable[] = {';      \
		  echo   'NULL,';                                      \
		  $(foreach N, $(LISTOFCANLAYERS),                     \
		    echo '    &$N_layer_modrec,';)                     \
		  echo '};';                                           \
		  echo 'CxsdLayerModRec   **remcxsd_layers = lyrtable;';\
		  echo 'int remcxsd_numlyrs = countof(lyrtable);';     \
		) >$@
#---------------------------------------------------------------------

#
EXES=				v4$(CANHAL_PFX)canserver
v4$(CANHAL_PFX)canserver_COMPONENTS=	v4$(CANHAL_PFX)canserver.o canserver_drivers.o \
				$(CANKOZ_LYR_SRCS:.c=.o) \
				$(CANKOZ_DRV_SRCS:.c=.o)

EXPORTSFILES2=	$(EXES)
EXPORTSDIR2=	lib/server/drivers

######################################################################
PRJDIR=         ../../..
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

GIVEN_DIR=	../socketcan
LOCAL_INCLUDES+=-I$(GIVEN_DIR)

v4$(CANHAL_PFX)canserver.%:	SPECIFIC_DEFINES=-DCANSERVER_ISSUE='"CXv4 SocketCAN canserver"'
LOCAL_GNTDFILES+=v4$(CANHAL_PFX)canserver.c canserver_drivers.c $(CANKOZ_DRV_SRCS)

#
v4$(CANHAL_PFX)canserver:	$(LIBREMSRV) \
				$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)
