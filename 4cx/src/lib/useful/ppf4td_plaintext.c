#include <stdio.h>

#include "ppf4td.h"
#include "ppf4td_plaintext.h"


static int ppf4td_plaintext_open (ppf4td_ctx_t *ctx, const char *reference)
{
    return (ctx->imp_privptr = fopen(reference, "r")) == NULL? -1 : 0;
}

static int ppf4td_plaintext_close(ppf4td_ctx_t *ctx)
{
  FILE *fp = ctx->imp_privptr;

    return fclose(fp);
}

static int ppf4td_plaintext_peekc(ppf4td_ctx_t *ctx, int *ch_p)
{
  FILE *fp = ctx->imp_privptr;

    *ch_p = fgetc(fp);
    if (*ch_p != EOF) ungetc(*ch_p, fp);

    if (ferror(fp)) return -1;
    if (feof  (fp)) return  0;
    return                 +1;
}

static int ppf4td_plaintext_nextc(ppf4td_ctx_t *ctx, int *ch_p)
{
  FILE *fp = ctx->imp_privptr;

    *ch_p = fgetc(fp);

    if (ferror(fp)) return -1;
    if (feof  (fp)) return  0;
    return                 +1;
}

static int  ppf4td_plaintext_init_m(void);
static void ppf4td_plaintext_term_m(void);

DEFINE_PPF4TD_PLUGIN(plaintext, "Plaintext PPF4TD plugin",
                     ppf4td_plaintext_init_m, ppf4td_plaintext_term_m,
                     ppf4td_plaintext_open,   ppf4td_plaintext_close,
                     ppf4td_plaintext_peekc,  ppf4td_plaintext_nextc,
                     PPF4TD_LINESYNC_NONE);

static int  ppf4td_plaintext_init_m(void)
{
    return ppf4td_register_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(plaintext)));
}

static void ppf4td_plaintext_term_m(void)
{
    ppf4td_deregister_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(plaintext)));
}
