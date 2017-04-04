######################################################################
PRJDIR=		../..
SECTION=	Config.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################

ifeq ":$(OS):$(CPU_X86_COMPAT):" ":LINUX:YES:"
  SUBDIRS=	cm5307_ppc_drvlets
else
  DUMMY:=	$(warning WARNING: Platform isn't LINUX-X86, so skipping build of cm5307_ppc_drvlets)
endif

######################################################################
include		$(TOPDIR)/TopRules.mk
######################################################################

LOCAL_BCKPFILES=src/*\~
