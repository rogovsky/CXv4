######################################################################
#                                                                    #
######################################################################

# ==== Just a sanity check ===========================================

ifeq "$(strip $(TOPDIR))" ""
  DUMMY:=	$(shell echo 'TopRules.mk: *** TOPDIR is unset' >&2)
  error		# Makes `make' stop with a "missing separator" error
endif


# ==== Configuration =================================================

include $(TOPDIR)/GeneralRules.mk

ifeq "$(OUTOFTREE)" ""
  TOP_INCLUDES=	-I$(TOPDIR)/include
else
  TOP_INCLUDES=	-I$(TOPEXPORTSDIR)/include
endif

# ====

#!!! Should port here "X11_LIBS_DIR"+"_L_MOTIF_LIB" woodoism from cx/*Rules.mk; but is that woodoism still needed?
MOTIF_LIBS:=	-L/usr/X11R6/lib \
		-lm -lXm -lXpm -lXt -lSM -lICE -lXext -lXmu -lX11


# ==== Additional, CXv4-specific rules ===============================

%_builtins.c:	%.modlist
		$(SHELL) $(TOPDIR)/scripts/mkbuiltins.sh -Mc <$< >$@


# ==== Directories ===================================================

TOPEXPORTSDIR=	$(TOPDIR)/../exports
TOPINSTALLDIR=	/tmp/4cx
# For autoconf-lovers -- they can use "make PREFIX=<path> install"
ifneq "$(PREFIX)" ""
  TOPINSTALLDIR=$(PREFIX)
endif

INSTALLSUBDIRS=	bin sbin configs include lib settings screens

INSTALL_bin_DIR=	$(TOPINSTALLDIR)/bin
INSTALL_sbin_DIR=	$(TOPINSTALLDIR)/sbin
INSTALL_configs_DIR=	$(TOPINSTALLDIR)/configs
INSTALL_include_DIR=	$(TOPINSTALLDIR)/include
INSTALL_lib_DIR=	$(TOPINSTALLDIR)/lib
INSTALL_settings_DIR=	$(TOPINSTALLDIR)/settings
INSTALL_screens_DIR=	$(TOPINSTALLDIR)/screens


# ==== Libraries =====================================================

ifeq "$(OUTOFTREE)" ""
  LIB_PREFIX_F= $(TOPDIR)/lib/$(1)/lib$(2).a
else
  LIB_PREFIX_F= $(TOPEXPORTSDIR)/lib/lib$(2).a
endif

LIBMISC:=       $(call LIB_PREFIX_F,misc,misc)
LIBUSEFUL:=     $(call LIB_PREFIX_F,useful,useful)
LIBCXSCHEDULER:=$(call LIB_PREFIX_F,useful,cxscheduler)

LIBXH:=		$(call LIB_PREFIX_F,Xh,Xh)
LIBXH_CXSCHEDULER:=	$(call LIB_PREFIX_F,Xh,Xh_cxscheduler)
LIBAUXMOTIFWIDGETS:=	$(call LIB_PREFIX_F,AuxMotifWidgets,AuxMotifWidgets)

LIBCX_ASYNC:=	$(call LIB_PREFIX_F,cxlib,cx_async)
LIBCX_SYNC:=	$(call LIB_PREFIX_F,cxlib,cx_sync)
LIBCDA:=	$(call LIB_PREFIX_F,cda,cda)
LIBCDA_D_INSRV:=$(call LIB_PREFIX_F,cda,cda_d_insrv)

LIBCXSD:=	$(call LIB_PREFIX_F,srv,cxsd)
LIBCXSD_PLUGINS:=	$(call LIB_PREFIX_F,srv,cxsd_plugins)
LIBREMDRVLET:=	$(call LIB_PREFIX_F,rem,remdrvlet)
LIBREMSRV:=	$(call LIB_PREFIX_F,rem,remsrv)

LIBDATATREE:=	$(call LIB_PREFIX_F,datatree,datatree)
LIBCDR:=	$(call LIB_PREFIX_F,Cdr,Cdr)
LIBKNOBSCORE:=	$(call LIB_PREFIX_F,KnobsCore,KnobsCore)
LIBMOTIFKNOBS:=	$(call LIB_PREFIX_F,MotifKnobs,MotifKnobs)
LIBMOTIFKNOBS_CDA:=	$(call LIB_PREFIX_F,MotifKnobs,MotifKnobs_cda)
LIBCHL:=	$(call LIB_PREFIX_F,Chl,Chl)

LIBPZFRAME_DATA:=	$(call LIB_PREFIX_F,pzframe,pzframe_data)
LIBPZFRAME_GUI:=	$(call LIB_PREFIX_F,pzframe,pzframe_gui)

LIBPZFRAME_DRV:=$(call LIB_PREFIX_F,pzframe,pzframe_drv)
LIBVDEV:=	$(call LIB_PREFIX_F,vdev,vdev)

LIBQT4CXSCHEDULER:=	$(call LIB_PREFIX_F,Qcxscheduler,Qt4cxscheduler)
LIBQT5CXSCHEDULER:=	$(call LIB_PREFIX_F,Qcxscheduler,Qt5cxscheduler)
