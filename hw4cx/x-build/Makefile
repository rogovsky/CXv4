######################################################################
PRJDIR=		..
SECTION=	Config.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

SUBDIRS=

# Value derived from ppc860-linux/x_rules.mk
PPC860_CANARY=$(PRJDIR)/../x-compile/ppc860-linux/tools/bin/ppc-linux-gcc
ifneq ":$(OS):$(CPU_X86_COMPAT):" ":LINUX:YES:"
  DUMMY:=	$(warning WARNING: skipping build of ppc860 because platform isn't LINUX-X86)
else
  ifeq "$(wildcard $(PPC860_CANARY))" ""
    DUMMY:=	$(warning WARNING: skipping build of ppc860 because of missing $(PPC860_CANARY))
  else
    MAY_BUILD_PPC860=YES
  endif
endif

# Value derived from arm-linux/x_rules.mk
ARM_CANARY=	$(PRJDIR)/../x-compile/arm-linux_1.3/bin/arm-linux-gcc
OS_GE_2_6_18:=	$(shell uname -r|awk 'BEGIN {FS="[-.]"}{print 2006017-($$1*1000000+$$2*1000+$$3)}'|sed 's/^-.*/YES/')
ifneq ":$(OS):$(CPU_X86_COMPAT):$(OS_GE_2_6_18):" ":LINUX:YES:YES:"
  DUMMY:=	$(warning WARNING: skipping build of arm because platform isn't Linux-X86 >=2.6.18)
else
  ifeq "$(wildcard $(ARM_CANARY))" ""
    DUMMY:=	$(warning WARNING: skipping build of arm because of missing $(ARM_CANARY))
  else
    MAY_BUILD_ARM=YES
  endif
endif

ifeq ":$(MAY_BUILD_PPC860):" ":YES:"
  SUBDIRS+=	ppc860-linux
endif
ifeq ":$(MAY_BUILD_ARM):" ":YES:"
  SUBDIRS+=	arm-linux
endif

######################################################################
include		$(TOPDIR)/TopRules.mk
######################################################################

LOCAL_BCKPFILES=arm-linux/*~ ppc860-linux/*~
