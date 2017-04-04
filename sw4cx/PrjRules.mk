TOPDIR=		$(PRJDIR)/../4cx/src

PRJ_INCLUDES=	-I$(PRJDIR)/include

ifeq "$(strip $(SECTION))" ""
  SECTION=	TopRules.mk
endif

ifeq "$(filter-out .% /%,$(SECTION))" ""
  include	$(SECTION)
else
  include	$(TOPDIR)/$(SECTION)
endif
