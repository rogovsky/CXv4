TANGO_BASE_DIR=			$(HOME)/compile/tango-9.2.5a
TANGO_SERVER_INCLUDE_DIR=	$(TANGO_BASE_DIR)/lib/cpp/server
TANGO_CLIENT_INCLUDE_DIR=	$(TANGO_BASE_DIR)/lib/cpp/client
TANGO_CLIENT_LIB_DIR=		$(TANGO_BASE_DIR)/lib/cpp/client/.libs
TANGO_LOG4TANGO_DIR=		$(TANGO_BASE_DIR)/lib/cpp/log4tango

TANGO_INCLUDES=		-I$(TANGO_SERVER_INCLUDE_DIR) \
			-I$(TANGO_CLIENT_INCLUDE_DIR) \
                        -I$(TANGO_LOG4TANGO_DIR)/include

ifeq "1" "1"
# Dynamic linking (with .so's)
  TANGO_LIBS=		-L$(TANGO_CLIENT_LIB_DIR) \
			-ltango -lomniORB4 -lomnithread \
			-lstdc++
else
# Static linking (with .a's)
# This branch does NOT work, because liblog4tango.a does NOT exist
  TANGO_LIBS=		$(TANGO_CLIENT_LIB_DIR)/libtango.a \
			$(TANGO_LOG4TANGO_DIR)/src/.libs/liblog4tango.a
			-lstdc++
endif


# /home/user/compile/tango-9.2.5a/lib/cpp/client/.libs/
# /home/user/compile/tango-9.2.5a/lib/cpp/server/tango.h
