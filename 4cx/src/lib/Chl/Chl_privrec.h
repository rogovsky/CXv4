#ifndef __CHL_PRIVREC_H
#define __CHL_PRIVREC_H


#include "datatree.h"
#include "datatreeP.h"
#include "Xh.h"

#include "Chl_knobprops_types.h"
#include "Chl_bigvals_types.h"
#include "Chl_histplot_types.h"
#include "Chl_help_types.h"


typedef struct
{
    CxWidget load_nfndlg;
    CxWidget save_nfndlg;
} stddlgs_rec_t;

typedef struct
{
    _m_histplot_update  proc;
    void               *privptr;
} chl_histinterest_cbinfo_t;

typedef struct
{
    DataSubsys       subsys;
    
    knobprops_rec_t  kp;
    bigvals_rec_t    bv;
    histplot_rec_t   hp;
    help_rec_t       hr;
    stddlgs_rec_t    dr;

    XhCommandProc    upper_commandproc;

    chl_histinterest_cbinfo_t *histinterest_cb_list;
    int                        histinterest_cb_list_allocd;
} chl_privrec_t;

chl_privrec_t *_ChlPrivOf(XhWindow win);


#endif /* __CHL_PRIVREC_H */
