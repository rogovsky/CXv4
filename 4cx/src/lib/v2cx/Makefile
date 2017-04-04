######################################################################
#                                                                    #
#  lib/v2cx/Makefile                                                 #
#      Rules for making Cx Data Access library                       #
#                                                                    #
######################################################################

NON_RNM_HDRS=	cx_proto.h cxdata.h Knobs_types.h Knobs_typesP.h
RENAMED_HDRS=	cx.h cxlib.h Knobs.h datatree.h cda.h Cdr.h
RENAMED_SYMS=	$(addprefix src_, $(RENAMED_HDRS))

# -- v2 cxlib

V2CXLIB_SRC_CS=		cxlib_client.c cxlib_utils.c cxlib_async.c
V2CXLIB_SRC_HS=		h2ii2h.h cxlib_wait_procs.h
V2CXLIB_SRC_IS=		cx.h cxlib.h

V2CXLIB_LIB_CS=		$(addprefix rnm_, $(V2CXLIB_SRC_CS))
V2CXLIB_SYM_CS=		$(addprefix src_, $(V2CXLIB_SRC_CS))
V2CXLIB_SYM_IS=		$(addprefix src_, $(V2CXLIB_SRC_IS))
V2CXLIB_SYM_HS=		$(V2CXLIB_SRC_HS)

# -- v2 datatree

V2DATATREE_SRC_CS=	datatree.c
V2DATATREE_SRC_IS=	cx.h Knobs.h datatree.h

V2DATATREE_LIB_CS=	$(addprefix rnm_, $(V2DATATREE_SRC_CS))
V2DATATREE_SYM_CS=	$(addprefix src_, $(V2DATATREE_SRC_CS))
V2DATATREE_SYM_IS=	$(addprefix src_, $(V2DATATREE_SRC_IS))

# -- v2 Cdr

V2CDR_SRC_CS=		Cdr.c descraccess.c
V2CDR_SRC_IS=		cx.h cda.h Knobs.h datatree.h Cdr.h

V2CDR_LIB_CS=		$(addprefix rnm_, $(V2CDR_SRC_CS))
V2CDR_SYM_CS=		$(addprefix src_, $(V2CDR_SRC_CS))
V2CDR_SYM_IS=		$(addprefix src_, $(V2CDR_SRC_IS))

# --------------------------------------------------------------------

ARLIBS=			libv2cx_async.a libv2cda.a libv2datatree.a libv2Cdr.a
libv2cx_async.a_COMPONENTS=	$(V2CXLIB_LIB_CS:.c=.o)
libv2cda.a_COMPONENTS=		stripped_cda.o
libv2datatree.a_COMPONENTS=	$(V2DATATREE_LIB_CS:.c=.o)
libv2Cdr.a_COMPONENTS=		$(V2CDR_LIB_CS:.c=.o)

UNCFILES=			v2all_COMPONENTS.mk

SPECIFIC_CFLAGS=	-fPIC

TOPDIR=         ../..
include $(TOPDIR)/TopRules.mk

# --------------------------------------------------------------------

V2CX_TOPDIR=	$(TOPDIR)/../../cx/src

$(NON_RNM_HDRS):	%:	$(V2CX_TOPDIR)/include/%
$(RENAMED_SYMS):	src_%:	$(V2CX_TOPDIR)/include/%

# -- v2 cxlib

$(V2CXLIB_SYM_CS):	src_%:		$(V2CX_TOPDIR)/lib/cxlib/%
$(V2CXLIB_SYM_HS):	%:		$(V2CX_TOPDIR)/lib/cxlib/%

$(V2CXLIB_LIB_CS):
	echo -e '#include "v2cxlib_renames.h"\n#include "$(strip $(patsubst rnm_%, src_%, $@))"' \
	>$@

$(V2CXLIB_LIB_CS:.c=.dep):	$(V2CXLIB_SYM_IS) $(NON_RNM_HDRS) $(V2CXLIB_SYM_HS)
$(V2CXLIB_LIB_CS:.c=.dep):	rnm_%.dep:	src_%.c

# -- v2 datatree

$(V2DATATREE_SYM_CS):	src_%:		$(V2CX_TOPDIR)/lib/Cdr/%

$(V2DATATREE_LIB_CS):
	echo -e '#include "v2datatree_renames.h"\n#include "$(strip $(patsubst rnm_%, src_%, $@))"' \
	>$@

$(V2DATATREE_LIB_CS:.c=.dep):	$(V2DATATREE_SYM_IS) $(NON_RNM_HDRS)
$(V2DATATREE_LIB_CS:.c=.dep):	rnm_%.dep:	src_%.c

# -- v2 Cdr

$(V2CDR_SYM_CS):	src_%:		$(V2CX_TOPDIR)/lib/Cdr/%

$(V2CDR_LIB_CS):
	echo -e '#include "v2Cdr_renames.h"\n#include "$(strip $(patsubst rnm_%, src_%, $@))"' \
	>$@

$(V2CDR_LIB_CS:.c=.dep):	$(V2CDR_SYM_IS) $(NON_RNM_HDRS) \
				$(V2CXLIB_SYM_IS) # Because of v2Cdr_renames.h::#include "v2cxlib_renames.h"
$(V2CDR_LIB_CS:.c=.dep):	rnm_%.dep:	src_%.c

# --- COMMON

SYMLINKS=	$(NON_RNM_HDRS) $(RENAMED_SYMS)\
		$(V2CXLIB_SYM_CS)    $(V2CXLIB_SYM_HS) \
		$(V2DATATREE_SYM_CS)                   \
		$(V2CDR_SYM_CS)
$(SYMLINKS):
		$(SCRIPTSDIR)/ln-sf_safe.sh $< $@
LOCAL_GNTDFILES+=	$(SYMLINKS) \
			$(V2CXLIB_LIB_CS) $(V2DATATREE_LIB_CS) $(V2CDR_LIB_CS)

# ---

v2all_COMPONENTS.mk:	$(ARLIBS) Makefile
		echo v2all_COMPONENTS=$$( (for f in $(ARLIBS); do ar t $$f; done) ) > $@

# #### END OF Makefile ###############################################
