######################################################################
#                                                                    #
#  programs/Makefile                                                 #
#      Rules for making CX programs/ subtree                         #
#                                                                    #
######################################################################

SUBDIRS=	
DRIVERS=	noop_drv.so remdrv_drv.so \
		test_rd_drv.so test_cda_del_drv.so \
		trig_read_drv.so mux_write_drv.so mirror_drv.so \
		arith_drv.so formula_drv.so
MONO_C_FILES=	$(DRIVERS)

TOPDIR=		../..
include DirRules.mk

# #### END OF Makefile ###############################################
