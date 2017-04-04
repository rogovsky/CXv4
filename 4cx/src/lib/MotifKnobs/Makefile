######################################################################
#                                                                    #
#  lib/MotifKnobs/Makefile                                           #
#      Rules for making Motif Knobs library                          #
#                                                                    #
######################################################################

ARLIBS=		libMotifKnobs.a libMotifKnobs_cda.a
libMotifKnobs.a_COMPONENTS=	MotifKnobs_internals.o \
				MotifKnobs_histplot.o \
				MotifKnobs_knobset.o \
				MotifKnobs_text_knob.o \
				MotifKnobs_selector_knob.o \
				MotifKnobs_choicebs_knob.o \
				MotifKnobs_alarmonoffled_knob.o \
				MotifKnobs_button_knob.o \
				MotifKnobs_text_text.o \
				MotifKnobs_label_noop.o \
				MotifKnobs_separator_noop.o \
				MotifKnobs_histplot_noop.o \
				MotifKnobs_grid_cont.o \
				MotifKnobs_subwin_cont.o \
				MotifKnobs_split_cont.o \
				MotifKnobs_scroll_cont.o \
				MotifKnobs_loggia_cont.o \
				MotifKnobs_canvas_cont.o \
				MotifKnobs_lrtb_grpg.o
libMotifKnobs_cda.a_COMPONENTS= MotifKnobs_cda_knobset.o \
				MotifKnobs_cda_scenario_knob.o \
				MotifKnobs_cda_leds.o \
				MotifKnobs_cda_leds_noop.o

EXPORTSFILES=	$(ARLIBS)
EXPORTSDIR=	lib

TOPDIR=         ../..
include $(TOPDIR)/TopRules.mk

# For ./Xm to be in <>-include-path, for Xm/XmStrDefsI.h to be accessible from /usr/include/Xm/Ext.h
LOCAL_INCLUDES=	-I.

# #### END OF Makefile ###############################################
