TYPES_LIST=	cac208 ceac124 cdac20 candac16 canadc40 \
		cgvi8 cpks8 curvv vsdc2 caniva \
		senkov_vip senkov_ebc weld02 \
		tvac320 panov_ubs \
		smc8 \
		\
		g0603 c0642 v0308 v06xx g0601 gzi11 v0628 c0612 c0609 l0403 \
		frolov_d16 dds300 dds500 u0632 \
		frolov_bfp frolov_bfp2 frolov_bfp16 frolov_bl700 frolov_spd \
		sukh_comm24 \
		\
		adc200me adc812me dl200me \
		adc200 adc502 adc333 adc4 c061621 \
		\
		slbpm ottcam ottucc \
		kshd485 \
		triadatv_um
DRV_INTERFACES= cankoz_common_drv_i.h kozdev_common_drv_i.h \
		$(foreach T, $(TYPES_LIST), $T_drv_i.h)
GIVEN_FILES=	$(DRV_INTERFACES)

EXPORTSFILES=	$(DRV_INTERFACES)
EXPORTSDIR=	include/drv_i

######################################################################
PRJDIR=		../..
include		$(PRJDIR)/PrjRules.mk
######################################################################
