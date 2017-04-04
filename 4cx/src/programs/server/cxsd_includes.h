/* 1. System includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

/* 2. Not-so-cx-specific, but useful includes */
#include "misc_types.h"
#include "misc_macros.h"
#include "misclib.h"

/* 3. Not-so-cx-specific, yet local, libraries' includes */
#include "cxscheduler.h"
#include "cxlogger.h"

/* 4. Portability issues */
#include "cx_sysdeps.h"

/* 5. CX-specific... */
#include "cx.h"

/* 6. Cxsd environment */

/*  .a Cxsd-library */
#include "cxsd_logger.h"
#include "cxsd_db.h"
#include "cxsd_hw.h"
#include "cxsd_modmgr.h"
#include "cxsd_driver.h"
#include "cxsd_frontend.h"
#include "cxsd_extension.h"

/*  .b cxsd daemon-specific */
#include "cxsd_vars.h"
#include "cxsd_config.h"
#include "main_builtins.h"
