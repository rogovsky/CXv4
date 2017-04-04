#include "cxsd_db_via_ppf4td.h"



/////////////////////////////////////////////////////////////////////
static CxsdDb file_ldr(const char *argv0,
                       const char *reference)
{
  ppf4td_ctx_t  ctx;

    if (ppf4td_open(&ctx, "m4", reference) != 0)
    {
        fprintf(stderr, "%s %s(): failed to open \"%s\": %s\n",
               strcurtime(), __FUNCTION__, reference, ppf4td_strerror(errno));
        return NULL;
    }

    return CxsdDbLoadDbViaPPF4TD(argv0, &ctx);
}


static int  file_dbldr_init_m(void);
static void file_dbldr_term_m(void);


DEFINE_CXSD_DBLDR(file, "CxsdDb-loader from file",
                  file_dbldr_init_m, file_dbldr_term_m,
                  file_ldr);

static int  file_dbldr_init_m(void)
{
    return CxsdDbRegisterDbLdr(&(CXSD_DBLDR_MODREC_NAME(file)));
}

static void file_dbldr_term_m(void)
{
    CxsdDbDeregisterDbLdr(&(CXSD_DBLDR_MODREC_NAME(file)));
}
