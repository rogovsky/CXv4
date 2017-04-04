#include "CdrP.h"

#include "Cdr_file_ldr.h"


DataSubsys  Cdr_file_subsys_ldr(const char *reference,
                                const char *argv0)
{
    return CdrLoadSubsystemFileViaPpf4td("!m4", reference, argv0);
}


static int  file_subsys_ldr_init_m(void);
static void file_subsys_ldr_term_m(void);

DEFINE_CDR_SUBSYS_LDR(file, "From-textfile Cdr subsystem-loader",
                      file_subsys_ldr_init_m, file_subsys_ldr_term_m,
                      Cdr_file_subsys_ldr);

static int  file_subsys_ldr_init_m(void)
{
    return CdrRegisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(file)));
}

static void file_subsys_ldr_term_m(void)
{
    CdrDeregisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(file)));
}
