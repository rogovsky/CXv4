#include <stdio.h>

#include "ppf4td.h"
#include "ppf4td_pipe.h"
#include "ppf4td_mem.h"


static int ppf4td_mem_open (ppf4td_ctx_t *ctx, const char *reference)
{
    ctx->imp_privptr = reference;

    return reference == NULL? -1 : 0;
}

static int ppf4td_mem_close(ppf4td_ctx_t *ctx)
{
    return 0;
}

static int ppf4td_mem_peekc(ppf4td_ctx_t *ctx, int *ch_p)
{
  const char *p = ctx->imp_privptr;

    *ch_p = *p;
    if (*ch_p == '\0') *ch_p = EOF;

    return *ch_p == EOF? 0 : +1;
}

static int ppf4td_mem_nextc(ppf4td_ctx_t *ctx, int *ch_p)
{
  const char *p = ctx->imp_privptr;

    *ch_p = *p;
    if (*ch_p == '\0') *ch_p = EOF;
    else
    {
        p++;
        ctx->imp_privptr = p;
    }

    return *ch_p == EOF? 0 : +1;
}

static int  ppf4td_mem_init_m(void);
static void ppf4td_mem_term_m(void);

DEFINE_PPF4TD_PLUGIN(mem, "From-Memory PPF4TD plugin",
                     ppf4td_mem_init_m, ppf4td_mem_term_m,
                     ppf4td_mem_open,   ppf4td_mem_close,
                     ppf4td_mem_peekc,  ppf4td_mem_nextc,
                     PPF4TD_LINESYNC_HLIN);

static int  ppf4td_mem_init_m(void)
{
    return ppf4td_register_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(mem)));
}

static void ppf4td_mem_term_m(void)
{
    ppf4td_deregister_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(mem)));
}
