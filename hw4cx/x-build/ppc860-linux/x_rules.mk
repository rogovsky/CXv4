ifeq "$(strip $(X_RULES_DIR))" ""
  DUMMY:=	$(shell echo 'x_rules.mk: *** X_RULES_DIR is unset' >&2)
  error		# Makes `make' stop with a "missing separator" error
endif

# ==== ppc860--specific setup/tuning =================================
CROSS_ROOT=	$(PRJDIR)/../x-compile/ppc860-linux
CROSS_TOOLS=	$(CROSS_ROOT)/tools
CROSS_ENVPFX=	CROSS_COMPILE=ppc-linux
override GCC=	$(CROSS_ENVPFX) $(CROSS_TOOLS)/bin/ppc-linux-gcc

DEBUGGABLE=	NO
PROFILABLE=	NO

######################################################################
include		$(TOPDIR)/TopRules.mk
######################################################################

# ==== Required replacements / local additions =======================
CPU=PPC860

ARCH_CFLAGS=	-mcpu=860 -msoft-float
ARCH_INCLUDES=	-isystem $(CROSS_ROOT)/include
ifeq "1" "1"
ARCH_INCLUDES+=	-isystem $(CROSS_ROOT)/tools/lib/gcc-lib/ppc-linux/3.2.2/include
endif
ARCH_LIBS=	-L$(CROSS_TOOLS)/lib -Wl,-rpath-link $(CROSS_TOOLS)/lib

# ====
LIBMISC=		$(X_RULES_DIR)/libmisc.a
LIBUSEFUL=		$(X_RULES_DIR)/libuseful.a
LIBCXSCHEDULER=		$(X_RULES_DIR)/libcxscheduler.a
LIBREMSRV=		$(X_RULES_DIR)/libremsrv.a
LIBREMDRVLET=		$(X_RULES_DIR)/libremdrvlet.a
LIBPZFRAME_DRV=		$(X_RULES_DIR)/libpzframe_drv.a
