VDEV_DRIVERS=	ist_cdac20_drv.so v1k_cdac20_drv.so v3h_a40d16_drv.so \
		mps20_ceac124_drv.so \
		kurrez_cac208_drv.so \
		iset_walker_drv.so \
		zebedev_subharmonic_drv.so \
		lebedev_lincamsel_drv.so lebedev_linsenctl_drv.so \
		lebedev_ringcamsel_drv.so lebedev_turnmag_drv.so \
		lebedev_avg_drv.so \
		double_iset_drv.so \
		vepp4_gimn_drv.so \
		v5k5045_drv.so \
		weldproc_drv.so
CDA_DRIVERS=	lebedev_subharmonic_drv.so liu_key_drv.so liu_doorilks_drv.so

MONO_C_FILES=	$(VDEV_DRIVERS) $(CDA_DRIVERS)

V4HDIR=		$(PRJDIR)/../hw4cx
LOCAL_INCLUDES=	-I$(V4HDIR)/include

######################################################################
PRJDIR=		..
SECTION=	programs/drivers/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################
$(VDEV_DRIVERS):	$(LIBVDEV)
