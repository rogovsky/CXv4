FASTADCS=	adc200me adc812me \
		adc200 adc502 adc333 adc4 c061621 f4226 \
		vsdc2_1 vsdc2_2 \
		adc4x250 #adc1000
VCAMIMGS=	ottcam
WIREBPMS=	u0632

DEVS=		$(FASTADCS) $(VCAMIMGS) $(WIREBPMS)
EXES=		$(DEVS) pzframeclient
# ----

DEVNNN_KNOBPLUGIN_FILES_F=	$(1)_data.o $(1)_gui.o $(1)_knobplugin.o
DEVNNN_FILES_F=			$(1).o $(1)_data.o $(1)_gui.o

# ADCs
adc200me_COMPONENTS=	$(call DEVNNN_FILES_F,adc200me)
adc812me_COMPONENTS=	$(call DEVNNN_FILES_F,adc812me)
adc200_COMPONENTS=	$(call DEVNNN_FILES_F,adc200)
adc502_COMPONENTS=	$(call DEVNNN_FILES_F,adc502)
adc333_COMPONENTS=	$(call DEVNNN_FILES_F,adc333)
adc4_COMPONENTS=	$(call DEVNNN_FILES_F,adc4)
c061621_COMPONENTS=	$(call DEVNNN_FILES_F,c061621)
f4226_COMPONENTS=	$(call DEVNNN_FILES_F,f4226)
vsdc2_2_COMPONENTS=	$(call DEVNNN_FILES_F,vsdc2_2)
vsdc2_1_COMPONENTS=	$(call DEVNNN_FILES_F,vsdc2_1)
adc4x250_COMPONENTS=	$(call DEVNNN_FILES_F,adc4x250)
adc1000_COMPONENTS=	$(call DEVNNN_FILES_F,adc1000)
# CCDs
ottcam_COMPONENTS=	$(call DEVNNN_FILES_F,ottcam)
# Misc
u0632_COMPONENTS=	$(call DEVNNN_FILES_F,u0632)

pzframeclient_COMPONENTS=	pzframeclient.o pzframeclient_knobset.o \
				$(foreach D, $(DEVS), \
				  $(call DEVNNN_KNOBPLUGIN_FILES_F, $D) \
				 )

UNCFILES=		$(foreach D, $(DEVS), \
			  $(call DEVNNN_KNOBPLUGIN_FILES_F, $D) \
			 )
DIR_DEPENDS=		$(UNCFILES:.o=.d)

LOCAL_LIBDEPS=	$(LIBPZFRAME_GUI) $(LIBPZFRAME_DATA)

LOCAL_GNTDFILES=pzframeclient.c pzframeclient_knobset.c \
		$(foreach D, $(FASTADCS) $(VCAMIMGS) $(WIREBPMS), \
		  $D.c $D_data.h $D_gui.h $D_knobplugin.h $D_knobplugin.c)

# --------------------------------------------------------------------
PRJDIR=		..
SUBDIRS=
#SECTION=	programs/xmclients/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
# --------------------------------------------------------------------

_SRC2CH_LINE=	sed -e 's/DEVTYPE_LCASE/$*/g' \
		    -e 's/DEVTYPE_UCASE/'$(shell echo $*|tr a-z A-Z)'/g' <$< >$@

$(foreach D, $(DEVS),     $D_data.h):       %_data.h:       SRC_ANYKIND_data.h
		$(_SRC2CH_LINE)
$(foreach D, $(DEVS),     $D_gui.h):        %_gui.h:        SRC_ANYKIND_gui.h
		$(_SRC2CH_LINE)
$(foreach D, $(DEVS),     $D_knobplugin.h): %_knobplugin.h: SRC_ANYKIND_knobplugin.h
		$(_SRC2CH_LINE)
#-
$(foreach D, $(FASTADCS), $D_knobplugin.c): %_knobplugin.c: SRC_FASTADC_knobplugin.c
		$(_SRC2CH_LINE)
$(foreach D, $(FASTADCS), $D.c):            %.c:            SRC_FASTADC.c
		$(_SRC2CH_LINE)
#-
$(foreach D, $(VCAMIMGS), $D_knobplugin.c): %_knobplugin.c: SRC_VCAMIMG_knobplugin.c
		$(_SRC2CH_LINE)
$(foreach D, $(VCAMIMGS), $D.c):            %.c:            SRC_VCAMIMG.c
		$(_SRC2CH_LINE)
#-
$(foreach D, $(WIREBPMS), $D_knobplugin.c): %_knobplugin.c: SRC_WIREBPM_knobplugin.c
		$(_SRC2CH_LINE)
$(foreach D, $(WIREBPMS), $D.c):            %.c:            SRC_WIREBPM.c
		$(_SRC2CH_LINE)

$(foreach D, $(DEVS), $D_data.dep):       %_data.dep:        %_data.h
$(foreach D, $(DEVS), $D_gui.dep):        %_gui.dep:         %_data.h
$(foreach D, $(DEVS), $D.dep):            %.dep:             %_gui.h
$(foreach D, $(DEVS), $D_gui.dep):        %_gui.dep:         %_gui.h
$(foreach D, $(DEVS), $D_knobplugin.dep): %_knobplugin.dep:  %_gui.h
$(foreach D, $(DEVS), $D_knobplugin.dep): %_knobplugin.dep:  %_knobplugin.h
pzframeclient_knobset.d: $(foreach D, $(DEVS), $D_knobplugin.h)

#!!!because of "v2cx::"; RHEL6: SPECIFIC_LIBS sufficient, RHEL7: DIR_LIBS
#pzframeclient:	SPECIFIC_LIBS=$(LIBDL)
pzframeclient_knobset.c: Makefile
		( \
		  echo \#include'"KnobsP.h"'; \
		  echo \#include'"pzframeclient_knobset.h"'; \
		  echo;                        \
		  $(foreach D, $(DEVS), echo \#include '"$D_knobplugin.h"';) \
		  echo;                        \
		  echo 'static knobs_knobset_t pzframeclient_knobset =';\
		  echo '{';\
		  echo '    (dataknob_unif_vmt_t *[]){';\
		  $(foreach D, $(DEVS), echo '        (dataknob_unif_vmt_t *)&$D_pzfr_vmt,';)\
		  echo '        NULL';\
		  echo '    },';\
		  echo '    NULL';\
		  echo '};';\
		  echo '';\
		  echo 'void RegisterPzframeclientKnobset(void)';\
		  echo '{';\
		  echo '    RegisterKnobset(&pzframeclient_knobset);';\
		  echo '}';\
		) >$@

pzframeclient.c: $(TOPDIR)/programs/xmclients/pult.c
	$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
pzframeclient.o pzframeclient.d: \
	SPECIFIC_DEFINES=-DSPECIFIC_KNOBSET_H_FILE='"pzframeclient_knobset.h"' \
	                 -DSPECIFIC_REGISTERKNOBSET_CODE='RegisterPzframeclientKnobset()'

# ---- "xmclients/DirRules.mk"
# but note: MOST files (besides pzframeclient)
#           should NOT depend on $(LIBCHL)
EXPORTSFILES=	$(EXES) $(MONO_C_FILES)
EXPORTSDIR=	bin

#!!!because of "v2cx::"; RHEL6: SPECIFIC_LIBS sufficient, RHEL7: DIR_LIBS
DIR_LIBS=	$(MOTIF_LIBS) $(LIBDL)
ifneq "$(strip $(EXES) $(MONO_C_FILES))" ""
  $(EXES) $(MONO_C_FILES):$(LOCAL_LIBDEPS) \
			$(LIBCHL) \
			$(LIBKNOBSCORE) $(LIBMOTIFKNOBS) $(LIBMOTIFKNOBS_CDA) \
			$(LIBCDR) $(LIBDATATREE) \
			$(LIBCDA) $(LIBCX_ASYNC) \
			$(LIBXH)  $(LIBAUXMOTIFWIDGETS) \
			$(LIBUSEFUL) $(LIBXH_CXSCHEDULER) $(LIBMISC)
endif
