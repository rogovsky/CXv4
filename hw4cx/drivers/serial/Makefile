######################################################################
PRJDIR=		../..
SECTION=	Config.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

SUBDIRS=        local com usb

# Value derived from arm-linux/x_rules.mk
ARM_CANARY=	$(PRJDIR)/../x-compile/arm-linux_1.3/bin/arm-linux-gcc
OS_GE_2_6_18:=	$(shell uname -r|awk 'BEGIN {FS="[-.]"}{print 2006017-($$1*1000000+$$2*1000+$$3)}'|sed 's/^-.*/YES/')
ifneq ":$(OS):$(CPU_X86_COMPAT):$(OS_GE_2_6_18):" ":LINUX:YES:YES:"
  DUMMY:=	$(warning WARNING: skipping build of MOXA UC-7112 drivers because platform isn't Linux-X86 >=2.6.18)
else
  ifeq "$(wildcard $(ARM_CANARY))" ""
    DUMMY:=	$(warning WARNING: skipping build of MOXA UC-7112 drivers because of missing $(ARM_CANARY))
  else
    MAY_BUILD_ARM=YES
  endif
endif

ifeq ":$(MAY_BUILD_ARM):" ":YES:"
  SUBDIRS+=	moxa
endif

######################################################################
include		$(TOPDIR)/TopRules.mk
######################################################################

LOCAL_BCKPFILES=src/*~ moxa/*~
