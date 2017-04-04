#include "CdrP.h"

#include "Cdr_file_ldr.h"


DataSubsys  Cdr_plaintext_subsys_ldr(const char *reference,
                                     const char *argv0)
{
    return CdrLoadSubsystemFileViaPpf4td("!plaintext", reference, argv0);
}


static int  plaintext_subsys_ldr_init_m(void);
static void plaintext_subsys_ldr_term_m(void);

DEFINE_CDR_SUBSYS_LDR(plaintext, "From-plaintext-file Cdr subsystem-loader",
                      plaintext_subsys_ldr_init_m, plaintext_subsys_ldr_term_m,
                      Cdr_plaintext_subsys_ldr);

static int  plaintext_subsys_ldr_init_m(void)
{
    return CdrRegisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(plaintext)));
}

static void plaintext_subsys_ldr_term_m(void)
{
    CdrDeregisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(plaintext)));
}
