#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "cx_sysdeps.h"
#include "misc_macros.h"
#include "misclib.h"
#include "memcasecmp.h"

#include "Knobs.h"
#include "KnobsP.h"


static void fprintf_stderr_knobpath(DataKnob k)
{
    if (k->uplink != NULL)
    {
        fprintf_stderr_knobpath(k->uplink);
        fprintf(stderr, ".");
    }
    fprintf(stderr, "%s", (k->ident != NULL)? k->ident : "(NULL-ident)");
}
    
static void reportproblem(const char *point, DataKnob k, const char *format, ...)
    __attribute__ ((format (printf, 3, 4)));

static void reportproblem(const char *point, DataKnob k, const char *format, ...)
{
  va_list ap;

    va_start(ap, format);
#if 1
    fprintf (stderr, "%s %s%s%s(",
             strcurtime(),
#if OPTION_HAS_PROGRAM_INVOCATION_NAME
             program_invocation_short_name, ": ",
#else
             "",                            "",
#endif
             point);
    fprintf_stderr_knobpath(k);
    fprintf (stderr, ", \"%s\"):", k->label != NULL? k->label : "");
    vfprintf(stderr, format, ap);
    fprintf (stderr, "\n");
#endif
    va_end(ap);
}


static dataknob_unif_vmt_t *FindVMT(int type, const char *name, size_t namelen)
{
  knobs_knobset_t      *knobset;
  dataknob_unif_vmt_t **item;
  
    for (knobset = first_knobset_ptr;
         knobset != NULL;
         knobset = knobset->next)
        for (item = knobset->list;
             *item != NULL  &&  (*item)->name != NULL;
             item++)
            if (
                (
                 (*item)->type == type
                 ||
                 (type == DATAKNOB_KNOB  &&  (*item)->type == DATAKNOB_NOOP)  // NOOPs may be used in place of KNOBs
                 ||
                 (type == DATAKNOB_GRPG  &&  (*item)->type == DATAKNOB_CONT)  // CONTs may be used in place of GRPGs
                )
                &&
                (
                 name == NULL  ||  *name == '\0'  ||
                 cx_strmemcasecmp((*item)->name, name, namelen) == 0
                )
               )
                return *item;

    return NULL;
}

dataknob_unif_vmt_t *GetKnobLookVMT(DataKnob k)
{
  dataknob_unif_vmt_t *vmt;
  const char          *lp;
  const char          *p;

    for (vmt = NULL, lp = k->look;
         vmt == NULL  &&  lp != NULL  &&  *lp != '\0';
         lp = p)
    {
        p = strchr(lp, ',');
        if (p == NULL) p = lp + strlen(lp);
        
        vmt = FindVMT(k->type, lp, p - lp);
        if (*p == ',') p++;
    }
    if (vmt == NULL)
    {
        vmt = FindVMT(k->type, DEFAULT_KNOB_LOOK_NAME, strlen(DEFAULT_KNOB_LOOK_NAME));
        if (k->look != NULL  &&  *k->look != '\0')
            reportproblem(__FUNCTION__, k,
                          "type=%d look=\"%s\" VMT not found",
                          k->type, k->look);
    }

    if (vmt == NULL)
        KnobsSetErr("%s: unable to find default VMT for type=%d",
                    __FUNCTION__, k->type);

    return vmt;
}


static void PurgeKnob(DataKnob k)
{
  dataknob_unif_vmt_t *vmt = k->vmtlink;

    if (vmt == NULL) return;
  
    /* Free resources... */
    if (vmt->privrec_size != 0)
    {
        if (vmt->options_table != NULL)
            psp_free(k->privptr, vmt->options_table);
            
        safe_free(k->privptr);
        k->privptr = NULL;
    }

    /* ...and delete the binding to VMT */
    k->vmtlink = NULL;
    vmt->ref_count--;
}

int       CreateKnob   (DataKnob k, CxWidget parent)
{
  dataknob_unif_vmt_t *vmt;
  int                  r;

    KnobsClearErr();

    if (k == NULL)
    {
        KnobsSetErr("Attempt to %s() a NULL knob", __FUNCTION__);
        return -1;
    }
    
    /* 0. Find a suitable knob implementation */
    vmt = GetKnobLookVMT(k);
    if (vmt == NULL)
        return -1;

    /* 1. Link the knob to appropriate VMT */
    vmt->ref_count++;
    k->vmtlink = vmt;
    
    /* 2. Allocate privrec and parse "options", if required */
    if (vmt->privrec_size != 0)
    {
        k->privptr = malloc(vmt->privrec_size);
        if (k->privptr == NULL)
        {
            KnobsSetErr("%s(%s/\"%s\"): unable to allocate %zu bytes for privrec",
                        __FUNCTION__, k->ident, k->label, vmt->privrec_size);
            return -1;
        }
        bzero(k->privptr, vmt->privrec_size);

        if (vmt->options_table != NULL)
        {
            r = psp_parse(k->options, NULL,
                          k->privptr,
                          Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                          vmt->options_table);
            if (r != PSP_R_OK)
            {
                reportproblem(__FUNCTION__, k,
                              " .options: %s",
                              psp_error());
                if (r == PSP_R_USRERR)
                    psp_parse("", NULL,
                              k->privptr,
                              Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                              vmt->options_table);
                else
                    bzero(k->privptr, vmt->privrec_size);
            }
        }
    }
    
    /* 3. Create a knob */
    r = vmt->Create(k, parent);
    if (r != 0)
    {
        DestroyKnob(k);
        
        reportproblem(__FUNCTION__, k,
                      "@Create(look=%s): =%d", vmt->name, r);
        return -1;
    }

    return 0;
}

void      DestroyKnob  (DataKnob k)
{
  int  n;
    
    /* Destroy children first */
    if (k->type == DATAKNOB_CONT)
        for (n = 0;  n < k->u.c.count; n++)
            DestroyKnob(k->u.c.content + n);

    /* Call the "destroy" method */
    if (k->vmtlink != NULL  &&
        k->vmtlink->Destroy != NULL)
        k->vmtlink->Destroy(k);

    PurgeKnob(k);
}


void RegisterKnobset(knobs_knobset_t *knobset)
{
    if (knobset->next != NULL) return;
    knobset->next     = first_knobset_ptr;
    first_knobset_ptr = knobset;
}

//// A hack for KnobsCore_simple to be always linked in
void *_KnobsCore_knobset_CreateSimpleKnob_ptr = CreateSimpleKnob;
