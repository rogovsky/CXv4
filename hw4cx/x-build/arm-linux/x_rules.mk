ifeq "$(strip $(X_RULES_DIR))" ""
  DUMMY:=       $(shell echo 'x_rules.mk: *** X_RULES_DIR is unset' >&2)
  error         # Makes `make' stop with a "missing separator" error
endif

# ==== arm-specific setup/tuning =====================================

#CROSS_ROOT=	/tmp/arm-linux_1.3/usr/local/arm-linux
CROSS_ROOT=	$(PRJDIR)/../x-compile/arm-linux_1.3
override GCC=	$(CROSS_ROOT)/bin/arm-linux-gcc

DEBUGGABLE=	NO
PROFILABLE=	NO

######################################################################
include		$(TOPDIR)/TopRules.mk
######################################################################

# ==== Required replacements / local additions =======================
CPU=ARM

# Preprocessor
ARCH_INCLUDES=	-isystem $(CROSS_ROOT)/include

# Loader
CROSS_LIBS=	$(CROSS_ROOT)/lib
# Note: libgcc.a is because of "__modsi3" problem -- for some reason, the linker doesn't pick libgcc by itself
ARCH_LIBS=	-L$(CROSS_LIBS) -nostdlib $(CROSS_LIBS)/libc.so.6 \
		$(CROSS_LIBS)/gcc-lib/arm-linux/3.3.2/libgcc.a \
		$(CROSS_LIBS)/crt1.o $(CROSS_LIBS)/crti.o $(CROSS_LIBS)/crtn.o \
		-Wl,-rpath-link $(CROSS_ROOT)/lib

# ====
LIBMISC=		$(X_RULES_DIR)/libmisc.a
LIBUSEFUL=		$(X_RULES_DIR)/libuseful.a
LIBCXSCHEDULER=		$(X_RULES_DIR)/libcxscheduler.a
LIBREMSRV=		$(X_RULES_DIR)/libremsrv.a
LIBREMDRVLET=		$(X_RULES_DIR)/libremdrvlet.a
LIBPZFRAME_DRV=		$(X_RULES_DIR)/libpzframe_drv.a
