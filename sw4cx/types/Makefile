TYPES_LIST=	ist_cdac20 v1k_cdac20 v3h_a40d16 mps20_ceac124 \
		iset_walker \
		lebedev_subharmonic \
		lebedev_lincamsel lebedev_linsenctl \
		lebedev_ringcamsel lebedev_turnmag \
		vepp4_gimn \
		liu_key liu_doorilks
TYPES_FILES=	$(foreach T, $(TYPES_LIST), $T.devtype)

EXPORTSFILES=	$(TYPES_FILES)
EXPORTSDIR=	configs

######################################################################
PRJDIR=		..
include		$(PRJDIR)/PrjRules.mk
######################################################################
