
# HW4DIR=="" => local, otherwise foreign
ifeq "$(HW4DIR)" ""
  HW4DIR=	$(PRJDIR)
  DIR_PATH=	.
else
  DIR_PATH=	$(HW4DIR)/drivers/vme/bivme2
endif

# VME
SRCDIR=		../src
VME_PFX=	bivme2
include		$(DIR_PATH)/../src/ShadowRules.mk

GIVEN_DIR=	$(HW4DIR)/given/bivme2

BIVME2_DEFINE_DRIVER_O=	$(DIR_PATH)/bivme2_DEFINE_DRIVER.o
BIVME2_IO_O=		$(DIR_PATH)/bivme2_io.o
LIBVMEDIRECT=		$(GIVEN_DIR)/libvmedirect.a

# ????????????
DRVLET_BINARIES=$(addsuffix .drvlet, \
		  $(addprefix $(VME_PFX)_, $(VMEDRIVERS)) \
		 )
DIR_DEPENDS=	$(DRVLET_BINARIES:.drvlet=_drv.d)
DIR_TARGETS=	$(DRVLET_BINARIES)

%.drvlet:	%_drv.o
		$(_O_BIN_LINE)

EXPORTSFILES=	$(DRVLET_BINARIES) $(MONO_C_FILES) $(UNCFILES)
EXPORTSDIR=	lib/server/drivers/bivme2_drvlets

LOCAL_DEFINES=	-DTRUE_DEFINE_DRIVER_H_FILE='"bivme2_DEFINE_DRIVER.h"'
LOCAL_INCLUDES=	-I$(DIR_PATH) -I$(GIVEN_DIR)

######################################################################
X_RULES_DIR=	$(HW4DIR)/x-build/ppc860-linux
include		$(X_RULES_DIR)/x_rules.mk
######################################################################

$(DRVLET_BINARIES):	$(BIVME2_DEFINE_DRIVER_O) $(BIVME2_IO_O) \
			$(LIBVMEDIRECT) \
			$(LIBPZFRAME_DRV) $(LIBREMDRVLET) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)
