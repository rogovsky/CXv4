######################################################################
#                                                                    #
#  lib/Makefile                                                      #
#      Rules for making CXv4 lib/ (libraries) subtree                #
#                                                                    #
######################################################################

CSUBDIRS=	misc useful \
		cxlib cda datatree Cdr KnobsCore \
		srv rem pzframe vdev
XSUBDIRS=	AuxMotifWidgets Xh MotifKnobs Chl
QSUBDIRS=	Qcxscheduler

ifneq "$(NOX)"  ""
  XSUBDIRS=	
endif
ifneq "$(NOQT)" ""
  QSUBDIRS=
endif
ifeq  "$(wildcard ../../../cx/src/include/cx.h)" ""
  NOV2CX=YES
  DUMMY:=	$(warning WARNING: ../../../cx/src/include/cx.h is absent, so skipping build of v2cx/)
endif
ifeq "$(NOV2CX)" ""
  CSUBDIRS+=	v2cx
endif

SUBDIRS=	$(CSUBDIRS) $(XSUBDIRS) $(QSUBDIRS)

LOCAL_BCKPFILES=Qcxscheduler/*~

######################################################################
TOPDIR=		..
include $(TOPDIR)/TopRules.mk
######################################################################

cda:		v2cx

# #### END OF Makefile ###############################################
