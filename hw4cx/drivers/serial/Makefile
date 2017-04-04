######################################################################
PRJDIR=		../..
SECTION=	Config.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

SUBDIRS=        local com usb

OS_GE_2_6_18:=	$(shell uname -r|awk 'BEGIN {FS="[-.]"}{print 2006017-($$1*1000000+$$2*1000+$$3)}'|sed 's/^-.*/YES/')
ifeq ":$(OS):$(CPU_X86_COMPAT):$(OS_GE_2_6_18):" ":LINUX:YES:YES:"
  MAY_BUILD_MOXA=YES
else
  DUMMY:=       $(warning WARNING: Platform isn't Linux-X86 >=2.6.18, so skipping build of MOXA UC-7112 drivers)
endif

ifeq ":$(MAY_BUILD_MOXA):" ":YES:"
  SUBDIRS+=	moxa
endif

######################################################################
include		$(TOPDIR)/TopRules.mk
######################################################################

LOCAL_BCKPFILES=src/*~ moxa/*~
