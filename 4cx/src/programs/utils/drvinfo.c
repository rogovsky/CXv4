#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include "cxsd_driver.h"


static int ShowDriverInfo(const char *argv0, const char *filename)
{
  int    ret = 0;
  void  *handle;
  char  *err;

  const char        *dname;
  static const char *suffx = "_drv.so"; /*!!!DIRSTRUCT*/
  size_t             dname_len;
  size_t             suffx_len;

  char               copy_dname[100];
  char               symbuf[200];
  CxsdDriverModRec  *metric;
  int                comma;
  
    /* Open the driver */
    handle = dlopen(filename, RTLD_LAZY);
    err    = dlerror();
    if (handle == NULL)
    {
        fprintf(stderr, "%s: unable to dlopen(\"%s\"): %s\n", argv0, filename, err);
        return 1;
    }

    /* Deduce driverrec name */
    dname = strrchr(filename, '/'); /*!!!DIRSTRUCT*/
    if (dname == NULL) dname = filename;
    else               dname++;
    dname_len = strlen(dname);
    suffx_len = strlen(suffx);
    if (dname_len < suffx_len  ||
        memcmp(dname + dname_len - suffx_len, suffx, suffx_len) != 0)
    {
        fprintf(stderr, "%s: can't deduce the driverrec name, since \"%s\" doesn't end with \"%s\"\n",
                argv0, dname, suffx);
        ret = 1;
        goto END_OF_FUNC;
    }

    dname_len -= suffx_len;
    if (dname_len > sizeof(copy_dname) - 1)
        dname_len = sizeof(copy_dname) - 1;
    if (dname_len != 0) memcpy(copy_dname, dname, dname_len);
    copy_dname[dname_len] = '\0';

    check_snprintf(symbuf, sizeof(symbuf), "%s%s", copy_dname, CXSD_DRIVER_MODREC_SUFFIX_STR);
    
    /* Get access to the metric */
    metric = dlsym(handle, symbuf);
    if (metric == NULL)
    {
        fprintf(stderr, "%s: symbol \"%s\" not found in \"%s\"\n",
                argv0, symbuf, filename);
        ret = 1;
        goto END_OF_FUNC;
    }

    /* Dump the data */
    printf("%s={\n", symbuf);
    
    printf("\tmagicnumber=0x%08x", metric->mr.magicnumber);
    if (metric->mr.magicnumber != CXSD_DRIVER_MODREC_MAGIC)
    {
        printf(" #invalid\n");
        goto END_OF_DUMP;
    }
    else
        printf("\n");

    printf("\tversion=%d.%d.%d.%d",
           CX_MAJOR_OF(metric->mr.version),
           CX_MINOR_OF(metric->mr.version),
           CX_PATCH_OF(metric->mr.version),
           CX_SNAP_OF (metric->mr.version));
    if (!CX_VERSION_IS_COMPATIBLE(metric->mr.version, CXSD_DRIVER_MODREC_VERSION))
    {
        printf(" #incompatible with %d.%d.%d.%d\n",
               CX_MAJOR_OF(CXSD_DRIVER_MODREC_VERSION),
               CX_MINOR_OF(CXSD_DRIVER_MODREC_VERSION),
               CX_PATCH_OF(CXSD_DRIVER_MODREC_VERSION),
               CX_SNAP_OF (CXSD_DRIVER_MODREC_VERSION));
        goto END_OF_DUMP;
    }
    else
        printf("\n");

    printf("\tname=\"%s\"\n\tcomment=\"%s\"\n",
           metric->mr.name, metric->mr.comment);
    printf("\tprivrecsize=%zu\n", metric->privrecsize);
    printf("\tparamtable=%s\n",  metric->paramtable == NULL? "EMPTY" : "PRESENT");
    printf("\tbusinfo_range=[%d,%d]\n", metric->min_businfo_n, metric->max_businfo_n);

    printf("\tchan_info=[%d]\n", metric->chan_nsegs);

    printf("\tlayer=");
    if (metric->layer == NULL)
        printf("NONE\n");
    else
        printf("\"%s\",%d.%d.%d.%d\n",
               metric->layer,
               CX_MAJOR_OF(metric->layerver),
               CX_MINOR_OF(metric->layerver),
               CX_PATCH_OF(metric->layerver),
               CX_SNAP_OF (metric->layerver));

    printf("\tdefined_methods={");
    comma = 0;
    if (metric->mr.init_mod != NULL) printf("%sinit_mod", comma? ", " : ""), comma = 1;
    if (metric->mr.term_mod != NULL) printf("%sterm_mod", comma? ", " : ""), comma = 1;
    if (metric->init_dev != NULL) printf("%sinit_dev", comma? ", " : ""), comma = 1;
    if (metric->term_dev != NULL) printf("%sterm_dev", comma? ", " : ""), comma = 1;
    if (metric->do_rw    != NULL) printf("%sdo_rw",    comma? ", " : ""), comma = 1;
    printf("}\n");
    
 END_OF_DUMP:
    printf("}\n");
     
 END_OF_FUNC:
    dlclose(handle);
    return ret;
}

int main(int argc, char *argv[])
{
  int  n;
  int  failcount;
    
    if (argc < 2)
    {
        printf("Usage: %s FILE_drv.so...\n", argv[0]);

        exit(1);
    }

    for (n = 1, failcount = 0; n < argc;  n++)
        failcount+= ShowDriverInfo(argv[0], argv[n]);

    return 0;
}
