EPICS_BASE_DIR=			$(HOME)/compile/base-3.15.6
EPICS_INCLUDE_DIR=		$(EPICS_BASE_DIR)/include
EPICS_OS_INCLUDE_DIR=		$(EPICS_INCLUDE_DIR)/os/Linux
EPICS_COMPILER_INCLUDE_DIR=	$(EPICS_INCLUDE_DIR)/compiler/gcc

#!!! Damn!!! Stupid EPICSoids -- one has to EXPLICITLY point to specific OS-ARCH dir, instead of something like "this platform"
EPICS_OS_LIB_DIR=		$(EPICS_BASE_DIR)/lib/linux-x86_64

EPICS_INCLUDES=			-I$(EPICS_INCLUDE_DIR) \
				-I$(EPICS_OS_INCLUDE_DIR) \
				-I$(EPICS_COMPILER_INCLUDE_DIR)

ifeq "1" "0"
  EPICS_LIBS=			-L$(EPICS_OS_LIB_DIR) \
				-lca -lCom
else
  EPICS_LIBS=			$(EPICS_OS_LIB_DIR)/libca.a \
				$(EPICS_OS_LIB_DIR)/libCom.a \
				-lpthread -lreadline -lrt -lstdc++ -lgcc_s -ltinfo
endif
