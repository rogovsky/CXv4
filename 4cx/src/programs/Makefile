######################################################################
#                                                                    #
#  programs/Makefile                                                 #
#      Rules for making CX programs/ subtree                         #
#                                                                    #
######################################################################

CSUBDIRS=	server drivers utils
XSUBDIRS=	tests xmclients runner pults
ifneq "$(NOX)" ""
  XSUBDIRS=	
endif
SUBDIRS=	$(CSUBDIRS) $(XSUBDIRS)

TOPDIR=		..
include $(TOPDIR)/TopRules.mk

# #### END OF Makefile ###############################################
