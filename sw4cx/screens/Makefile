CLIENTS=	rfsyn_eng v5rfsyn v5ie v5kls v5cnv \
		linmag linvac linthermcan linslbpms linipp lin485s linrf_smc \
		ringmag ringcor23 ringcor45 ringrf ringcurmon \
		subharmonic lincamsel ringcamsel turnmag \
		v5h1adc200s \
		v5adc200_gun  v5adc200_kls1 v5adc200_kls2 v5adc200_kls3 \
		v5adc200_grp1 v5adc200_grp2 ionchamber \
		sukhpanel blm v5adc200_kkrs \
		ttv \
		ist_cdac20 v1k_cdac20 v3h_a40d16 mps20_ceac124 \
		k500gid25s \
		gid25x4 v4gid25s v4k500mag \
		vepp34info vepp5info \
		v5ringcurmon4v4 \
		baachi verona weldcc xweldcc
AUX_SCREEN_FILES=	subsys_magx_macros.m4 gid25_subsys_macros.m4

EXPORTSFILES3=	weld_mes_up_mv2mka.lst
EXPORTSDIR3=	settings/common

#---
CLIENTS+=	liu
V4HDIR=		$(PRJDIR)/../hw4cx
LOCAL_BCKPFILES+= panov_ubs.m4 kshd485.m4 dl200me_f.m4
AUX_SCREEN_FILES+=panov_ubs.m4 kshd485.m4 dl200me_f.m4
#---
######################################################################
PRJDIR=		..
SECTION=	programs/pults/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
######################################################################
#---
panov_ubs.m4:	$(V4HDIR)/screens/panov_ubs.subsys
		cp -p $< $@
kshd485.m4:	$(V4HDIR)/screens/kshd485.subsys
		cp -p $< $@
dl200me_f.m4:	$(V4HDIR)/screens/dl200me_f.subsys
		cp -p $< $@
liu:		panov_ubs.m4 kshd485.m4 dl200me_f.m4
#---
