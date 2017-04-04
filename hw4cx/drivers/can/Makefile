######################################################################
PRJDIR=		../..
SECTION=	Config.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

SUBDIRS=        local marathon-2.4

ifeq ":$(OS):$(CPU_X86_COMPAT):" ":LINUX:YES:"
  MAY_BUILD_C4LCANGW=YES
else
  $(warning WARNING: Platform isn't LINUX-X86, so skipping build of c4l-cangw/canserver)
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
  SUBDIRS+=	socketcan rtcan
endif

######################################################################
include		$(TOPDIR)/TopRules.mk
######################################################################

LOCAL_BCKPFILES=src/*~ socketcan/*~ c4l-cangw/*~
