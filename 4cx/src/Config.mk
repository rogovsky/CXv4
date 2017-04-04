######################################################################
#                                                                    #
#  Config.mk                                                         #
#      Sets config parameters, pulling them out of Sysdepflags.sh    #
#                                                                    #
#  Requires:                                                         #
#      TOPDIR                                                        #
#                                                                    #
#      Can be used either from *Rules.mk or from some other Makefile #
#  *before* inclusion of *Rules.mk (e.g., to determine set of        #
#  targets and/or subdirectories, depending on OS/CPU).              #
#      Forces single inclusion (by use of CONFIG_MK_INCLUDED).       #
#                                                                    #
######################################################################

ifeq "$(CONFIG_MK_INCLUDED)" ""

CONFIG_MK_INCLUDED=	YES

SCRIPTSDIR=	$(TOPDIR)/scripts

SYSFLAGS:=	$(shell $(SCRIPTSDIR)/Sysdepflags.sh)

OS:=		$(strip $(subst <OS>=,  ,$(filter <OS>=%,  $(SYSFLAGS))))
CPU:=		$(strip $(subst <CPU>=, ,$(filter <CPU>=%, $(SYSFLAGS))))

CPU_X86_COMPAT=$(strip $(if $(filter $(CPU), X86 X86_64), YES,))

# Whether we are called from within the main CX tree.
# Idea is the following: all CX makefiles set TOPDIR to a series of "../",
# so that if TOPDIR contains anything but [./], we are out of the tree.
OUTOFTREE:=
ifneq "$(strip $(subst .,,$(subst /,,$(TOPDIR))))" ""
  OUTOFTREE:=	YES
endif

endif # CONFIG_MK_INCLUDED
