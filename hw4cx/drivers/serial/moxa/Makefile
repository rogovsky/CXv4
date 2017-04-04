SRCDIR=		../src
SERIAL_TYPE=	moxa
SERIALDEV_PFX=	M
include		$(SRCDIR)/ShadowRules.mk

#--- SerialserverShadowRules.mk --------------------------------------
LISTOFDRIVERS=	$(SRCDIR)/LISTOFSERIALDRIVERS.mk
MAKEFILE_PARTS+=$(LISTOFDRIVERS)
include		$(LISTOFDRIVERS)
#
LISTOFSERIALLAYERS=	$(SERIAL_TYPE)piv485
SERIAL_LYR_SRCS=	$(addsuffix _lyr.c, $(LISTOFSERIALLAYERS))
#
SERIAL_DRV_SRCS=	$(addsuffix _drv.c, $(LISTOFSERIALDRIVERS))
SERIAL_SYMLINKS=	$(SERIAL_DRV_SRCS)
$(SERIAL_SYMLINKS):	%: $(SRCDIR)/%
			$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
#
##v4$(SERIAL_TYPE)moxaserver.c:		$(SRCDIR)/serialserver_common.c
##			$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
#
serialserver_drivers.c:	$(LISTOFDRIVERS)  Makefile
		(                                                      \
		  echo \#include'"remsrv.h"';                          \
		  echo;                                                \
		  $(foreach N, $(LISTOFSERIALDRIVERS),                 \
		    echo 'extern CxsdDriverModRec $N_driver_modrec;';) \
		  echo 'static CxsdDriverModRec * drvtable[] = {';     \
		  $(foreach N, $(LISTOFSERIALDRIVERS),                 \
		    echo '    &$N_driver_modrec,';)                    \
		  echo '    NULL';                                     \
		  echo '};';                                           \
		  echo 'CxsdDriverModRec **remsrv_drivers = drvtable;';\
		  echo;                                                \
		  $(foreach N, $(LISTOFSERIALLAYERS),                  \
		    echo 'extern CxsdLayerModRec $N_layer_modrec;';)   \
		  echo 'static CxsdLayerModRec * lyrtable[] = {';      \
		  echo   'NULL,';                                      \
		  $(foreach N, $(LISTOFSERIALLAYERS),                  \
		    echo '    &$N_layer_modrec,';)                     \
		  echo '};';                                           \
		  echo 'CxsdLayerModRec   **remcxsd_layers = lyrtable;';\
		  echo 'int remcxsd_numlyrs = countof(lyrtable);';     \
		) >$@
#---------------------------------------------------------------------
MONO_C_FILES=	moxasetupkshd485 moxadumpserial
EXES=		cxv4moxaserver

cxv4moxaserver_COMPONENTS=	cxv4moxaserver.o serialserver_drivers.o \
				$(SERIAL_LYR_SRCS:.c=.o) \
				$(SERIAL_DRV_SRCS:.c=.o)

EXPORTSFILES=	$(MONO_C_FILES) $(EXES)
EXPORTSDIR=	lib/server/drivers/moxa_arm_drvlets

LOCAL_DEFINES=	-DMOXA_KSHD485

######################################################################
PRJDIR=		../../..
X_RULES_DIR=	$(PRJDIR)/x-build/arm-linux

SECTION=	$(X_RULES_DIR)/x_rules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################


LOCAL_GNTDFILES+=serialserver_drivers.c

#
cxv4moxaserver:	$(LIBREMSRV) \
		$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)
$(SERIAL_TYPE)setupkshd485 $(SERIAL_TYPE)dumpserial:	$(LIBMISC)
