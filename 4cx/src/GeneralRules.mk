######################################################################
#                                                                    #
#  GeneralRules.mk                                                   #
#  Version 4.0                                                       #
#      Common rules for large projects                               #
#                                                                    #
#  WARNING:                                                          #
#      It is suitable only for GNU `make',                           #
#      usual `make' wouldn't work.                                   #
#                                                                    #
#  Recommended usage:                                                #
#      Makefile[<-DirRules.mk]<-$(TOP)/TopRules.mk<-GeneralRules.mk  #
#                                                                    #
#  Input parameters:                                                 #
#  (those marked with "+" are TopRules.mk's responsibility;          #
#   "." are to be supplied by user via command line)                 #
#     +SCRIPTSDIR      where the Sysdepflags.sh script lives         #
#      SUBDIRS                                                       #
#                                                                    #
#      EXES            list of executables to be built               #
#      SOLIBS          list of .so-files to be built                 #
#      ARLIBS          list of .a-files to be built                  #
#      UNCFILES        list of uncotegorized files to be built       #
#                                                                    #
#      MAKEFILE_PARTS  list of local makefiles besides "Makefile"    #
#                                                                    #
#     .OPTIMIZATION    default is -O2, set to -O0 for debugging      #
#     .DEBUGGABLE                                                    #
#     .AUXWARNS        =all turns on lots of auxilliary -W's         #
#                                                                    #
#    Installation subsystem:                                         #
#                                                                    #
#      INSTALLSUBDIRS  list of install subdirs' "names"              #
#                                                                    #
#                                                                    #
#  Output parameters:                                                #
#                                                                    #
#                                                                    #
#  Version history:                                                  #
#      4.x - CXv4's GeneralRules.mk                                  #
#      3.x - skipped                                                 #
#      2.x - CXv2's (work/cx/) Rules.mk                              #
#      1.x - CXv1's (work/oldcx) Rules.mk                            #
#      0.x - UCAM's (work/ucam) Makefile                             #
#                                                                    #
#                                                                    #
######################################################################

# ==== Required spells ===============================================

.PHONY:		firsttarget
firsttarget:	all


# ==== Configuration =================================================

ifneq "$(MAKEFILES)" ""
  THIS:=	$(notdir $(word $(words $(MAKEFILE_LIST)), $(MAKEFILE_LIST)))
else
  THIS:=	GeneralRules.mk
endif

include		$(TOPDIR)/Config.mk
ifeq "$(OS)" "UNKNOWN"
  DUMMY:=	$(shell echo '$(THIS): *** OS type is UNKNOWN' >&2)
  error
endif


# ==== File lists ====================================================

COMPOSITE_TARGETS=	$(EXES) $(SOLIBS) $(ARLIBS) $(LOCAL_COMPOSITE_TARGETS)
MONOLITHIC_TARGETS=	$(MONO_C_FILES)

TARGETFILES=	$(COMPOSITE_TARGETS) $(MONOLITHIC_TARGETS) $(UNCFILES) $(DIR_TARGETS)


# ==== Environment ===================================================

# ---- General -------------------------------------------------------
NOP=		@echo >/dev/null

# ---- Debuggability -------------------------------------------------
ifeq "$(DEBUGGABLE)" ""
  DEBUGGABLE=	YES
endif

ifeq "$(DEBUGGABLE)" "NO"
  ifeq "$(OS)" "IRIX"
    LD_STRIP_FLAG=
  else
    LD_STRIP_FLAG=	-s
  endif
  CC_DEBUG_FLAG=
  LD_DEBUG_FLAG=
else
  CC_DEBUG_FLAG=	-g
  LD_DEBUG_FLAG=	-g
endif

# ---- And profilability ---------------------------------------------
ifeq "$(PROFILABLE)" "yes"
  override PROFILABLE=YES
endif

ifeq "$(PROFILABLE)" "YES"
  CC_PROFILE_FLAG=	-pg
  LD_PROFILE_FLAG=	-pg
endif


# ---- GCC specifics -------------------------------------------------
ifeq "$(GCC)" ""
  GCC=		gcc
endif
GCCCMD=		$(GCC) $(GCCFLAGS)

# GCC version, required for dependency generation (damn!!!)
ISNEWGCC:=	$(shell echo "\#if __GNUC__*1000+__GNUC_MINOR__>2095zYESz\#elsezNOz\#endif" | tr 'z' '\n' | $(GCCCMD) -E - | grep -v '^[^A-Z]' | grep -v '^$$')
# The same for "-Wno-missing-field-initializers"
GCCHASWNMFI:=	$(shell echo "\#if __GNUC__*1000+__GNUC_MINOR__>=4000zYESz\#elsezNOz\#endif" | tr 'z' '\n' | $(GCCCMD) -E - | grep -v '^[^A-Z]' | grep -v '^$$')

# ---- Compiler ------------------------------------------------------
CC=		$(GCCCMD)
ACT_CFLAGS=	$(strip $(CFLAGS) $(RQD_CFLAGS) $(TOP_CFLAGS) $(DIR_CFLAGS) $(SHD_CFLAGS) $(ARCH_CFLAGS) $(PRJ_CFLAGS) $(LOCAL_CFLAGS) $(SPECIFIC_CFLAGS))

CFLAGS=		$(OPTIMIZATION) $(AUXWARNS) -W -Wall $(WARNINGS) $(CC_DEBUG_FLAG) $(CC_PROFILE_FLAG)
RQD_CFLAGS=

OPTIMIZATION=	-O2

WARNINGS=	
WARNINGS=	-Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wshadow
ifeq "$(GCCHASWNMFI)" "YES"
  WARNINGS+=	-Wno-missing-field-initializers
endif
AUXWARNS=
ifeq "$(AUXWARNS)" "all"
  override AUXWARNS = -Wpointer-arith -Wbad-function-cast \
			-Wcast-align -Wwrite-strings \
                        -Wmissing-declarations \
                        -Winline -Wpadded
endif

# ---- Preprocessor --------------------------------------------------
CPP=		$(CC) -E
ACT_CPPFLAGS=	$(strip $(CPPFLAGS) $(RQD_CPPFLAGS) $(TOP_CPPFLAGS) $(DIR_CPPFLAGS) $(SHD_CPPFLAGS) $(ARCH_CPPFLAGS) $(PRJ_CPPFLAGS) $(LOCAL_CPPFLAGS) $(SPECIFIC_CPPFLAGS))

CPPFLAGS=
ifeq "$(OS)" "LINUX"
  CPPFLAGS=	-D_GNU_SOURCE
endif
RQD_CPPFLAGS=	$(ACT_DEFINES) $(ACT_INCLUDES)

ACT_DEFINES=	$(strip $(RQD_DEFINES)  $(TOP_DEFINES)  $(DIR_DEFINES)  $(SHD_DEFINES)  $(ARCH_DEFINES)  $(PRJ_DEFINES)  $(LOCAL_DEFINES) $(SPECIFIC_DEFINES))
RQD_DEFINES=	-DOS_$(OS) -DCPU_$(CPU)

ACT_INCLUDES=	$(strip $(RQD_INCLUDES) $(TOP_INCLUDES) $(DIR_INCLUDES) $(SHD_INCLUDES) $(ARCH_INCLUDES) $(PRJ_INCLUDES) $(LOCAL_INCLUDES) $(SPECIFIC_INCLUDES))
RQD_INCLUDES=	

# ---- Linker --------------------------------------------------------
LD=		$(GCCCMD)
ACT_LDFLAGS=	$(strip $(LDFLAGS) $(RQD_LDFLAGS) $(TOP_LDFLAGS) $(DIR_LDFLAGS) $(SHD_LDFLAGS) $(ARCH_LDFLAGS) $(PRJ_LDFLAGS) $(LOCAL_LDFLAGS) $(SPECIFIC_LDFLAGS))

LDFLAGS=	$(LD_STRIP_FLAG) $(LD_DEBUG_FLAG) $(LD_PROFILE_FLAG)
RQD_LDFLAGS=	

# "ACT_LIBS" has non-standard ordering of components since libraries' order matters
ACT_LIBS=	$(LIBS) $(SPECIFIC_LIBS) $(LOCAL_LIBS) $(PRJ_LIBS) $(DIR_LIBS) $(SHD_LIBS) $(ARCH_LIBS) $(TOP_LIBS) $(RQD_LIBS)
RQD_LIBS=	
ifneq "$(findstring z$(OS)z, zSOLARISz zUNIXWAREz)"  ""
  RQD_LIBS=	-lsocket -lnsl
endif
LIBDL=
ifneq "$(findstring z$(OS)z, zLINUXz)"  ""
  LIBDL=	-ldl
endif

# ---- Archiver ------------------------------------------------------
AR=		ar
ACT_ARFLAGS=	$(strip $(ARFLAGS) $(RQD_ARFLAGS) $(TOP_ARFLAGS) $(DIR_ARFLAGS) $(SHD_ARFLAGS) $(ARCH_ARFLAGS) $(PRJ_ARFLAGS) $(LOCAL_ARFLAGS) $(SPECIFIC_ARFLAGS))

ARFLAGS=
RQD_ARFLAGS=	rc

RANLIB=		ranlib
ifeq "$(OS)" "IRIX"
  RANLIB=	ar rs
endif
ifeq "$(OS)" "UNIXWARE"
  RANLIB=	$(NOP)
endif

# ---- Maker ---------------------------------------------------------
MAKESUBDIR=	$(MAKE) -C

# note: we HAVE to put ';' right into the value, otherwise /bin/sh barks
#       "line 0: syntax error near unexpected token `;'"
#       (yes, /bin/sh IS stupid)
ifeq "$(findstring k,$(firstword $(MAKEFLAGS)))" ""
  SET-E=	set -e;
else
  SET-E=
endif

# ---- File lists for cleaning ---------------------------------------
RQD_GNTDFILES=	*.d *.mkr
RQD_BCKPFILES=	*\~ .*\~
RQD_INTMFILES=	*.o *.dep
COREFILES=	core core.[^h]* *.core
RQD_WORKFILES=	

ACT_GNTDFILES=	$(strip $(RQD_GNTDFILES) $(TOP_GNTDFILES) $(DIR_GNTDFILES) $(SHD_GNTDFILES) $(ARCH_GNTDFILES) $(PRJ_GNTDFILES) $(LOCAL_GNTDFILES))
ACT_BCKPFILES=	$(strip $(RQD_BCKPFILES) $(TOP_BCKPFILES) $(DIR_BCKPFILES) $(SHD_BCKPFILES) $(ARCH_BCKPFILES) $(PRJ_BCKPFILES) $(LOCAL_BCKPFILES))
ACT_INTMFILES=	$(strip $(RQD_INTMFILES) $(TOP_INTMFILES) $(DIR_INTMFILES) $(SHD_INTMFILES) $(ARCH_INTMFILES) $(PRJ_INTMFILES) $(LOCAL_INTMFILES))
ACT_WORKFILES=	$(strip $(RQD_WORKFILES) $(TOP_WORKFILES) $(DIR_WORKFILES) $(SHD_WORKFILES) $(ARCH_WORKFILES) $(PRJ_WORKFILES) $(LOCAL_WORKFILES))

FILES4clean=		$(COREFILES) $(ACT_INTMFILES) $(TARGETFILES)
FILES4distclean=        $(FILES4clean) $(ACT_BCKPFILES) $(ACT_WORKFILES)
FILES4maintainer-clean=	$(FILES4distclean) $(ACT_GNTDFILES)
FILES4crit-clean=	$(FILES4maintainer-clean) $(CRITCLEANFILES)


# ==== Inference rules ===============================================

# Disable built-in suffix rules
.SUFFIXES:

# Compilation of C-files
_C_O_LINE=	$(CC) -o $@ $<   $(ACT_CPPFLAGS) -c $(ACT_CFLAGS)
%.o: %.c
		$(_C_O_LINE)

# Linkage of dynamically loaded (shared) objects
%.so: %.o
		$(LD)   -o $@ $^ -shared -Wl,-soname,$@ $(ACT_LDFLAGS)
                #$(ACT_LIBS) -lc

# Autogeneration of dependencies
%.d: %.dep
		sed   's/$*.o *:/$*.o $@:/' <$*.dep >$@

ifeq "$(ISNEWGCC)" "YES"
%.dep: %.c
		$(CC)   -MM $(ACT_CPPFLAGS) $< -MF$@ >/dev/null
else
%.dep: %.c
		$(CC)   -MM $(ACT_CPPFLAGS) $< -o $@
endif

# ???Dealing with components
_MKR_LINE=	echo "$*: $($*_COMPONENTS) $($*_LIBDEPS)" >$@
$(addsuffix .mkr, $(ARLIBS)):	_MKR_LINE=	echo "$*: $*($(strip $($*_COMPONENTS) $($*_GIVEN_COMPONENTS)))" >$@
%.mkr:		Makefile $(MAKEFILE_PARTS)
		$(_MKR_LINE)

# Archives
(%): %
		$(AR) $(ACT_ARFLAGS) $@ $^
		$(RANLIB) $@

# -Note-: possibly a very dangerous (match-anything) rule!!!
# Unfortunately there *must* be a '%.o' dependency, else `make' becomes mad.
_O_BIN_LINE=	$(LD)   -o $@ $^ $(ACT_LDFLAGS) $(ACT_LIBS)
%: %.o
		$(_O_BIN_LINE)


# ==== Some commonly-used phony targets ==============================

# ---- Main (files/dirs) dependencies --------------------------------

.PHONY:		all files subdirs $(SUBDIRS)

all:		files subdirs

files:		$(TARGETFILES) $(GIVEN_FILES)

subdirs:	$(SUBDIRS)

ifneq "$(strip $(SUBDIRS))" ""
$(SUBDIRS):
		$(MAKESUBDIR) $@

  DO_SUBDIR_TARGET=	$(SET-E)  for i in $(SUBDIRS); do $(MAKESUBDIR) $$i $@; done
else
  DO_SUBDIR_TARGET=	
endif


# ---- Directory cleaning rules --------------------------------------

.PHONY:		clean distclean maintainer-clean crit-clean

clean distclean maintainer-clean crit-clean:
		rm -f $(FILES4$@)
		$(DO_SUBDIR_TARGET)

# ---- Publishing-related --------------------------------------------

.PHONY:		create-exports exports install

ifeq "$(OUTOFTREE)" ""

create-exports:
		rm -rf $(TOPEXPORTSDIR)
		for i in `cat $(TOPDIR)/EXPORTSTREE`; do \
			$(SCRIPTSDIR)/mkdir-p.sh $(TOPEXPORTSDIR)/$$i;\
			done

#!!! Note: since "cp -Rp" can turn out to be not-very-portable behaviour
#          (despite being specified by POSIX),
#          it can be wise to replace "cp" with some form of "tar".
exports:	all
		$(if $(strip $(EXPORTSFILES)),  cp -Rp $(EXPORTSFILES)  $(TOPEXPORTSDIR)/$(EXPORTSDIR))
		$(if $(strip $(EXPORTSFILES2)), cp -Rp $(EXPORTSFILES2) $(TOPEXPORTSDIR)/$(EXPORTSDIR2))
		$(if $(strip $(EXPORTSFILES3)), cp -Rp $(EXPORTSFILES3) $(TOPEXPORTSDIR)/$(EXPORTSDIR3))
		$(DO_SUBDIR_TARGET)

install:	exports
		$(foreach S, $(INSTALLSUBDIRS), \
			echo "INSTALLING $S"  &&  \
			$(SCRIPTSDIR)/mkdir-p.sh $(INSTALL_$S_DIR)  &&  \
			$(SCRIPTSDIR)/copydir.sh $(TOPEXPORTSDIR)/$S $(INSTALL_$S_DIR)  &&  \
		)  true
else

# Note: this cryptic code does a very simple thing: if the 1st component
#       of EXPORTSDIR is one of $(INSTALLSUBDIRS), it is replaced with actual
#       value of corresponding INSTALL_nnn_DIR and files are placed there,
#       otherwise files are placed to $(TOPINSTALLDIR)/$(EXPORTSDIR).
#       This is done for a case when some subdirs are copied to locations
#       out of $(TOPINSTALLDIR) -- this code performs correct subdir
#       translation.

EMPTY:=
SPACE_C:= $(EMPTY) $(EMPTY)

_OI_SPLT1=	$(subst /,$(SPACE_C),$(EXPORTSDIR))
_OI_FIRST1=	$(word 1, $(_OI_SPLT1))
_OI_REST1=	$(subst $(SPACE_C),/,$(wordlist 2, $(words $(_OI_SPLT1)) , $(_OI_SPLT1)))

_OI_SPLT2=	$(subst /,$(SPACE_C),$(EXPORTSDIR2))
_OI_FIRST2=	$(word 1, $(_OI_SPLT2))
_OI_REST2=	$(subst $(SPACE_C),/,$(wordlist 2, $(words $(_OI_SPLT2)) , $(_OI_SPLT2)))

_OI_SPLT3=	$(subst /,$(SPACE_C),$(EXPORTSDIR3))
_OI_FIRST3=	$(word 1, $(_OI_SPLT3))
_OI_REST3=	$(subst $(SPACE_C),/,$(wordlist 2, $(words $(_OI_SPLT3)) , $(_OI_SPLT3)))

install:	all
		P="$(INSTALL_$(_OI_FIRST1)_DIR)"; \
			if [ "$(strip $(EXPORTSFILES))" != "" -a "$(EXPORTSDIR)" != "" ]; then \
				cp -Rp $(EXPORTSFILES) $${P:-$(TOPINSTALLDIR)/$(_OI_FIRST1)}/$(_OI_REST1); \
			fi
		P="$(INSTALL_$(_OI_FIRST2)_DIR)"; \
			if [ "$(strip $(EXPORTSFILES2))" != "" -a "$(EXPORTSDIR2)" != "" ]; then \
				cp -Rp $(EXPORTSFILES2) $${P:-$(TOPINSTALLDIR)/$(_OI_FIRST2)}/$(_OI_REST2); \
			fi
		P="$(INSTALL_$(_OI_FIRST3)_DIR)"; \
			if [ "$(strip $(EXPORTSFILES3))" != "" -a "$(EXPORTSDIR3)" != "" ]; then \
				cp -Rp $(EXPORTSFILES3) $${P:-$(TOPINSTALLDIR)/$(_OI_FIRST3)}/$(_OI_REST3); \
			fi
		$(DO_SUBDIR_TARGET)

endif


# ---- Some useful targets -------------------------------------------

.PHONY:		flags nothing

flags:
		@$(foreach V, \
		OS CPU CPU_X86_COMPAT ISNEWGCC GCCHASWNMFI OUTOFTREE, \
		printf  '%-16s "%s"\n'  "$V:"  "$($V)"; )

nothing:
		$(NOP)


# ==== Auto-dependencies =============================================

COMPOSITE_DEPENDS=	$(addsuffix .d, $(basename $(foreach T, $(COMPOSITE_TARGETS), $($T_COMPONENTS))))
MONOLITHIC_DEPENDS=	$(addsuffix .d, $(basename $(MONOLITHIC_TARGETS)))
ALLDEPENDS=	$(COMPOSITE_DEPENDS) $(MONOLITHIC_DEPENDS) \
		$(DIR_DEPENDS) $(LOCAL_DEPENDS)
ALLTARGETRULES=	$(addsuffix .mkr, $(COMPOSITE_TARGETS))

ifneq "$(strip $(ALLDEPENDS) $(ALLTARGETRULES))" ""
  include $(ALLDEPENDS) $(ALLTARGETRULES)
endif

# #### END OF GeneralRules.mk ########################################
