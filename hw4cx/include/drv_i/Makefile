TYPES_LIST=	cac208 ceac124 cdac20 candac16 canadc40 \
		slio24_reg \
		cgvi8 cpks8 curvv vsdc2 caniva canipp \
		senkov_vip senkov_ebc weld02 \
		tvac320 panov_ubs \
		smc8 \
		\
		c0601 c0609 c0612 c0642 \
		g0401 g0601 g0603 gzi11 \
		i0633 \
		l0403 \
		r0601 \
		u0632 \
		v0308 \
		v06xx v0628 \
		dds300 dds500 \
		frolov_d16 frolov_ie4 \
		frolov_bfp frolov_bfp2 frolov_bfp16 frolov_bl700 frolov_spd \
		frolov_k4x1 frolov_k4x1_2x2x1 \
		sukh_comm24 \
		\
		adc200 adc502 adc333 adc4 c061621 f4226 \
		\
		adc200me adc812me dl200me \
		\
		adc4x250 adc1000 \
		\
		slbpm ottcam ottucc \
		kshd485 \
		triadatv_um \
		\
		vadc16 vdac20
DRV_INTERFACES= cankoz_common_drv_i.h kozdev_common_drv_i.h \
		$(foreach T, $(TYPES_LIST), $T_drv_i.h)
GIVEN_FILES=	$(DRV_INTERFACES)

EXPORTSFILES=	$(DRV_INTERFACES)
EXPORTSDIR=	include/drv_i

######################################################################
PRJDIR=		../..
include		$(PRJDIR)/PrjRules.mk
######################################################################
