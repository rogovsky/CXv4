######################################################################
PRJDIR=		../..
SECTION=	Config.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

# Value derived from ppc860-linux/x_rules.mk
PPC860_CANARY=$(PRJDIR)/../x-compile/ppc860-linux/tools/bin/ppc-linux-gcc
ifneq ":$(OS):$(CPU_X86_COMPAT):" ":LINUX:YES:"
  DUMMY:=	$(warning WARNING: skipping build of bivme2 because platform isn't LINUX-X86)
else
  ifeq "$(wildcard $(PPC860_CANARY))" ""
    DUMMY:=	$(warning WARNING: skipping build of bivme2 because of missing $(PPC860_CANARY))
  else
    SUBDIRS=	bivme2 mbivme2
  endif
endif

######################################################################
include		$(TOPDIR)/TopRules.mk
######################################################################

LOCAL_BCKPFILES=src/*\~ nsrc/*\~
