######################################################################
PRJDIR=		../..
SECTION=	Config.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

SUBDIRS=        local marathon-2.4 udpcan

# Value derived from ppc860-linux/x_rules.mk
PPC860_CANARY=$(PRJDIR)/../x-compile/ppc860-linux/tools/bin/ppc-linux-gcc
ifneq ":$(OS):$(CPU_X86_COMPAT):" ":LINUX:YES:"
  DUMMY:=	$(warning WARNING: skipping build of c4l-cangw/canserver because platform isn't LINUX-X86)
else
  ifeq "$(wildcard $(PPC860_CANARY))" ""
    DUMMY:=	$(warning WARNING: skipping build of c4l-cangw/canserver because of missing $(PPC860_CANARY))
  else
    MAY_BUILD_C4LCANGW=YES
  endif
endif

OS_GE_2_6_25:=	$(shell uname -r|awk 'BEGIN {FS="[-.]"}{print 2006024-($$1*1000000+$$2*1000+$$3)}'|sed 's/^-.*/YES/')
ifeq ":$(OS):$(OS_GE_2_6_25):" ":LINUX:YES:"
  MAY_BUILD_SOCKETCAN=YES
else
  DUMMY:=	$(warning WARNING: Platform isn't Linux >=2.6.25, so skipping build of SocketCAN drivers)
endif

ifeq ":$(MAY_BUILD_C4LCANGW):" ":YES:"
  SUBDIRS+=	c4l-cangw
endif
ifeq ":$(MAY_BUILD_SOCKETCAN):" ":YES:"
  SUBDIRS+=	socketcan sktcanserver rtcan
endif

######################################################################
include		$(TOPDIR)/TopRules.mk
######################################################################

LOCAL_BCKPFILES=src/*~ socketcan/*~ sktcanserver/*~ rtcan/*~ c4l-cangw/*~
