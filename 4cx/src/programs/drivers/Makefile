######################################################################
#                                                                    #
#  programs/Makefile                                                 #
#      Rules for making CX programs/ subtree                         #
#                                                                    #
######################################################################

SUBDIRS=	
DRIVERS=	noop_drv.so remdrv_drv.so \
		test_rd_drv.so test_cda_del_drv.so \
		trig_read_drv.so trig_exec_drv.so \
		mux_write_drv.so mirror_drv.so \
		preset_selector_drv.so \
		arith_drv.so formula_drv.so
MONO_C_FILES=	$(DRIVERS)

UNCFILES=	test_rem_rd_rw.drvlet
#EXPORTSFILES2=	test_rem_rd_rw.drvlet
#EXPORTSDIR2=	$(EXPORTSDIR)
LOCAL_DEPENDS=	test_rem_rd_rw_drv.d

TOPDIR=		../..
include DirRules.mk

test_rem_rd_rw.drvlet:	%.drvlet:	%_drv.o
		$(_O_BIN_LINE)

test_rem_rd_rw.drvlet:	$(LIBREMDRVLET) \
			$(LIBUSEFUL) $(LIBCXSCHEDULER) $(LIBMISC)

# #### END OF Makefile ###############################################
