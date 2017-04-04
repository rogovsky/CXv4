# "CLIENTS" -- space-separated list of clients, w/o ".subsys"
# "AUX_SCREEN_FILES"

.PHONY:		firsttarget
firsttarget:	all

LIST_OF_PULTS_FILE=	LIST_OF_PULTS.lst
DEF_PULT_CLIENT=	pult
ifneq "$(wildcard $(LIST_OF_PULTS_FILE))" ""
#  CLIENT_OF_SCREEN_F=	$(shell awk -F' ' \
#			'BEGIN                          {CLIENT="$(DEF_PULT_CLIENT)"} \
#			 (P=$$1) && ("$(strip $1)" ~ P) {CLIENT=$$2}    \
#			 END                            {print CLIENT}' \
#			$(LIST_OF_PULTS_FILE))
  CLIENT_OF_SCREEN_F=	$(shell awk -F' ' \
			'BEGIN               {CLIENT="$(DEF_PULT_CLIENT)"} \
			 "$(strip $1)" ~ $$1 {CLIENT=$$2}    \
			 END                 {print CLIENT}' \
			$(LIST_OF_PULTS_FILE))
else
  CLIENT_OF_SCREEN_F=	$(DEF_PULT_CLIENT)
endif

ifneq "$(strip $(CLIENTS))" ""
$(CLIENTS): 	$(wildcard $(LIST_OF_PULTS_FILE))
		N=$(call CLIENT_OF_SCREEN_F, $@); \
		[ "$$N" == "-" ]  ||  $(SCRIPTSDIR)/ln-sf_safe.sh $$N $@
endif

UNCFILES=	$(CLIENTS)

EXPORTSFILES=	$(foreach C, $(CLIENTS), $C.subsys) $(AUX_SCREEN_FILES)
EXPORTSDIR=	screens

EXPORTSFILES2=	$(CLIENTS)
EXPORTSDIR2=	bin

######################################################################
include $(TOPDIR)/TopRules.mk
######################################################################
