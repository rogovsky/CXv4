SRVHOSTS=	linac1 ring1 foxy v2k-k500-1 vepp4-pult6 \
		rack0 rack1
CLNHOSTS=	linac3 worker1 camilla2 io io2 vepp4-pult6

linac1_SERVERS=		11 13 14 15 16 17 18 19 22
ring1_SERVERS=		12 21
foxy_SERVERS=		20
v2k-k500-1_SERVERS=	23
vepp4-pult6_SERVERS=	0 2
rack0_SERVERS=		24 26
rack1_SERVERS=		23

DEVLISTS=	$(foreach H, $(SRVHOSTS), \
		  $(foreach N, $($(H)_SERVERS), devlist-$H-$N.lst))
STARTERCONFS=	$(foreach H, $(CLNHOSTS), cx-starter-$H.conf) srvparams.conf

GIVEN_FILES=	devlist_magx_macros.m4 $(DEVLISTS) $(STARTERCONFS)

EXPORTSFILES=	$(GIVEN_FILES)
EXPORTSDIR=	configs

######################################################################
PRJDIR=		..
include		$(PRJDIR)/PrjRules.mk
######################################################################
