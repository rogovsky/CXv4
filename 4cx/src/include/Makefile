######################################################################
#                                                                    #
#  include/Makefile                                                  #
#                                                                    #
#                                                                    #
######################################################################

SUBDIRS=		sysdeps pixmaps
TOPDIR=		..
include $(TOPDIR)/TopRules.mk

# ==== Regular .h-files ==============================================

EXPORTSFILES=	misc_types.h misclib.h misc_sepchars.h misc_macros.h fps_counter.h \
		fix_arpa_inet.h \
		cx_module.h cx_version.h \
		paramstr_parser.h fdiolib.h cxscheduler.h \
		cxlogger.h \
		cxldr.h main_builtins.h \
		findfilein.h sendqlib.h ppf4td.h \
		memcasecmp.h timeval_utils.h \
		\
		cx_sysdeps.h endianconv.h \
		\
		cxsd_driver.h \
		cxsd_db.h cxsd_dbP.h cxsd_hw.h cxsd_hwP.h \
		cxsd_extension.h cxsd_frontend.h \
		cxsd_logger.h cxsd_modmgr.h \
		\
		cx.h cxlib.h \
		remdrv_proto_v4.h remsrv.h remdrvlet.h remcxsd.h remcxsdP.h \
		cda.h cdaP.h \
		Xh.h XhP.h cx_common_types.h Xh_fallbacks.h Xh_globals.h \
		Xh_viewport.h Xh_plot.h plotdata.h \
                Xh_monoimg.h \
		Cdr.h CdrP.h datatree.h datatreeP.h \
		Chl.h ChlP.h \
		Knobs.h KnobsP.h \
		MotifKnobsP.h MotifKnobs_cda.h MotifKnobs_cda_leds.h \
		MotifKnobs_histplot.h \
		\
		pzframe_data.h  pzframe_gui.h  pzframe_knobplugin.h  pzframe_main.h \
		fastadc_data.h  fastadc_gui.h  fastadc_knobplugin.h  fastadc_main.h \
		vcamimg_data.h  vcamimg_gui.h  vcamimg_knobplugin.h  vcamimg_main.h \
		wirebpm_data.h  wirebpm_gui.h  wirebpm_knobplugin.h  wirebpm_main.h \
		pzframe_cpanel_decor.h \
		\
		pzframe_drv.h vdev.h
EXPORTSDIR=	include

# ==== AuxMotifWidgets .h-files ======================================

AUXMOTIFHEADERS=	IncDecB.h \
			InputOnly.h \
			XmLiteClue.h \
			XmSepGridLayout.h

$(AUXMOTIFHEADERS):
			$(SCRIPTSDIR)/ln-sf_safe.sh ../lib/AuxMotifWidgets/$@ $@

files:			$(AUXMOTIFHEADERS)

CRITCLEANFILES=		$(AUXMOTIFHEADERS)

# #### END OF Makefile ###############################################
