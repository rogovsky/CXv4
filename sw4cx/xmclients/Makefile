LOCAL_DEVS=	two812ch
V4HPZ_DEVS=	adc200me adc812me
DEVS=		$(LOCAL_DEVS) $(V4HPZ_DEVS)

WELD_NOOPS=	weldclient_process

EXES=		$(LOCAL_DEVS) liuclient weldclient

# ----

DEVNNN_KNOBPLUGIN_FILES_F=	$(1)_data.o $(1)_gui.o $(1)_knobplugin.o
DEVNNN_FILES_F=			$(1).o $(1)_data.o $(1)_gui.o

two812ch_COMPONENTS=	$(call DEVNNN_FILES_F,two812ch)
liuclient_COMPONENTS=	liuclient.o liuclient_knobset.o \
			$(foreach D, $(LOCAL_DEVS), \
			  $(call DEVNNN_KNOBPLUGIN_FILES_F, $D) \
			)
weldclient_COMPONENTS=	weldclient.o weldclient_knobset.o \
			$(foreach D, $(WELD_NOOPS), $D_noop.o)

UNCFILES=		$(foreach D, $(LOCAL_DEVS), \
			  $(call DEVNNN_KNOBPLUGIN_FILES_F, $D) \
			)
#!!! vvv - WTF?!  Why that is required
DIR_DEPENDS=            $(UNCFILES:.o=.d)
                                                   

V4HDIR=		$(PRJDIR)/../hw4cx
PZDIR=		$(V4HDIR)/pzframes
LOCAL_INCLUDES=	-I$(PZDIR)
LOCAL_LIBDEPS=	$(LIBPZFRAME_GUI) $(LIBPZFRAME_DATA)
LOCAL_GNTDFILES=liuclient.c liuclient_knobset.c weldclient_knobset.c


# --------------------------------------------------------------------
PRJDIR=		..
SUBDIRS=
#SECTION=	programs/xmclients/DirRules.mk
include		$(PRJDIR)/PrjRules.mk
# --------------------------------------------------------------------

# liuclient

ADCNNN_KNOBPLUGIN_FILES_F=	$(PZDIR)/$(strip $(1))_data.o \
				$(PZDIR)/$(strip $(1))_gui.o \
				$(PZDIR)/$(strip $(1))_knobplugin.o

liuclient:	$(foreach D, $(V4HPZ_DEVS), \
		  $(call ADCNNN_KNOBPLUGIN_FILES_F, $D))

#!!!because of "v2cx::"; RHEL6: SPECIFIC_LIBS sufficient, RHEL7: DIR_LIBS
#liuclient:	SPECIFIC_LIBS=$(LIBDL)
liuclient_knobset.c: Makefile
		( \
		  echo \#include'"KnobsP.h"'; \
		  echo \#include'"liuclient_knobset.h"'; \
		  echo;                        \
		  $(foreach D, $(DEVS), echo \#include '"$D_knobplugin.h"';) \
		  echo;                        \
		  echo 'static knobs_knobset_t liuclient_knobset =';\
		  echo '{';\
		  echo '    (dataknob_unif_vmt_t *[]){';\
		  $(foreach D, $(DEVS), echo '        (dataknob_unif_vmt_t *)&$D_pzfr_vmt,';)\
		  echo '        NULL';\
		  echo '    },';\
		  echo '    NULL';\
		  echo '};';\
		  echo '';\
		  echo 'void RegisterLiuclientKnobset(void)';\
		  echo '{';\
		  echo '    RegisterKnobset(&liuclient_knobset);';\
		  echo '}';\
		) >$@

liuclient.c: $(TOPDIR)/programs/xmclients/pult.c
	$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
liuclient.o liuclient.d: \
	SPECIFIC_DEFINES=-DSPECIFIC_KNOBSET_H_FILE='"liuclient_knobset.h"' \
	                 -DSPECIFIC_REGISTERKNOBSET_CODE='RegisterLiuclientKnobset()'

# weldclient

weldclient:	weldclient_process_noop.o

#!!!because of "v2cx::"; RHEL6: SPECIFIC_LIBS sufficient, RHEL7: DIR_LIBS
#weldclient:	SPECIFIC_LIBS=$(LIBDL)
weldclient_knobset.c: Makefile
		( \
		  echo \#include'"KnobsP.h"'; \
		  echo \#include'"weldclient_knobset.h"'; \
		  echo;                        \
		  $(foreach D, $(WELD_NOOPS), echo \#include '"$D_noop.h"';) \
		  echo;                        \
		  echo 'static knobs_knobset_t weldclient_knobset =';\
		  echo '{';\
		  echo '    (dataknob_unif_vmt_t *[]){';\
		  $(foreach D, $(WELD_NOOPS), echo '        (dataknob_unif_vmt_t *)&$D_noop_vmt,';)\
		  echo '        NULL';\
		  echo '    },';\
		  echo '    NULL';\
		  echo '};';\
		  echo '';\
		  echo 'void RegisterWeldclientKnobset(void)';\
		  echo '{';\
		  echo '    RegisterKnobset(&weldclient_knobset);';\
		  echo '}';\
		) >$@

weldclient.c: $(TOPDIR)/programs/xmclients/pult.c
	$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
weldclient.o weldclient.d: \
	SPECIFIC_DEFINES=-DSPECIFIC_KNOBSET_H_FILE='"weldclient_knobset.h"' \
	                 -DSPECIFIC_REGISTERKNOBSET_CODE='RegisterWeldclientKnobset()'

# ---- "xmclients/DirRules.mk"
EXPORTSFILES=   $(EXES) $(MONO_C_FILES)
EXPORTSDIR=     bin

#!!!because of "v2cx::"; RHEL6: SPECIFIC_LIBS sufficient, RHEL7: DIR_LIBS
DIR_LIBS=       $(MOTIF_LIBS) $(LIBDL)
ifneq "$(strip $(EXES) $(MONO_C_FILES))" ""
  $(EXES) $(MONO_C_FILES):$(LOCAL_LIBDEPS) \
			$(LIBCHL) \
			$(LIBKNOBSCORE) $(LIBMOTIFKNOBS) $(LIBMOTIFKNOBS_CDA) \
			$(LIBCDR) $(LIBDATATREE) \
			$(LIBCDA) $(LIBCX_ASYNC) \
			$(LIBXH)  $(LIBAUXMOTIFWIDGETS) \
			$(LIBUSEFUL) $(LIBXH_CXSCHEDULER) $(LIBMISC)
endif
