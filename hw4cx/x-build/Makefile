######################################################################
PRJDIR=		..
SECTION=	Config.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

SUBDIRS=

ifeq ":$(OS):$(CPU_X86_COMPAT):" ":LINUX:YES:"
  MAY_BUILD_PPC860=YES
else
  DUMMY:=	$(warning WARNING: Platform isn't LINUX-X86, so skipping build of ppc860)
endif

OS_GE_2_6_18:=	$(shell uname -r|awk 'BEGIN {FS="[-.]"}{print 2006017-($$1*1000000+$$2*1000+$$3)}'|sed 's/^-.*/YES/')
ifeq ":$(OS):$(CPU_X86_COMPAT):$(OS_GE_2_6_18):" ":LINUX:YES:YES:"
  MAY_BUILD_ARM=YES
else
  DUMMY:=	$(warning WARNING: Platform isn't Linux-X86 >=2.6.18, so skipping build of arm)
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
