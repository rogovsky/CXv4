#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <math.h>

#include "misc_macros.h"
#include "misclib.h"
#include "memcasecmp.h"

#include "datatree.h"
#include "datatreeP.h"


static knob_editstate_hook_t editstate_hook = NULL;

void set_knob_editstate_hook(knob_editstate_hook_t hook)
{
    editstate_hook = hook;
}


rflags_t    choose_knob_rflags(DataKnob k, rflags_t rflags, double new_v)
{
  dataknob_knob_data_t *kd      = &(k->u.k);

    if (k == NULL)                return rflags;
    
    //  0. Do "``changed by...'' flags" have sense?
    if (!k->is_rw  ||  (k->behaviour & DATAKNOB_B_IGN_OTHEROP) != 0)
        rflags &=~ CXCF_FLAG_4WRONLY_MASK;

    //  2. ALARM
    if ((k->behaviour & DATAKNOB_B_IS_ALARM) != 0  &&
        ( ((int)new_v) & CX_VALUE_LIT_MASK  ) != 0)
        rflags |= CXCF_FLAG_ALARM_ALARM;

    if (k->type != DATAKNOB_KNOB) return rflags;

    //  3. YELLOW...
    if (kd->num_params > DATAKNOB_PARAM_NORM_MAX
        &&
        (kd->params[DATAKNOB_PARAM_NORM_MIN].value < kd->params[DATAKNOB_PARAM_NORM_MAX].value)
        &&
        (
         new_v < kd->params[DATAKNOB_PARAM_NORM_MIN].value
         ||
         new_v > kd->params[DATAKNOB_PARAM_NORM_MAX].value
        ))
        rflags |= CXCF_FLAG_COLOR_YELLOW;
    //    ...and RED
    if (kd->num_params > DATAKNOB_PARAM_YELW_MAX
        &&
        (kd->params[DATAKNOB_PARAM_YELW_MIN].value < kd->params[DATAKNOB_PARAM_YELW_MAX].value)
        &&
        (
         new_v < kd->params[DATAKNOB_PARAM_YELW_MIN].value
         ||
         new_v > kd->params[DATAKNOB_PARAM_YELW_MAX].value
        ))
        rflags |= CXCF_FLAG_COLOR_RED;

    // 4. DEFUNCT -- is now in cda responsibility

    // 5. "Attention" -- RELAX and OTHEROP: still a question...
    // (11.11.2015)5. "Attention" -- in fact, now only RELAX
    if (k->attn_endtime != 0)
    {
        if (k->attn_state == KNOBSTATE_RELAX)
            rflags |= CXCF_FLAG_ALARM_RELAX;
    }
    
    return rflags;
}

knobstate_t choose_knobstate(DataKnob k, rflags_t rflags)
{
  knobstate_t  state = KNOBSTATE_NONE;

    if (rflags & CXCF_FLAG_PRGLYCHG)     state = KNOBSTATE_PRGLYCHG;
    if (k != NULL  &&  k->attn_endtime != 0)
                                         state = k->attn_state;
    if (rflags & CXCF_FLAG_COLOR_YELLOW) state = KNOBSTATE_YELLOW;
    if (rflags & CXCF_FLAG_COLOR_RED)    state = KNOBSTATE_RED;
    if (rflags & CXCF_FLAG_COLOR_WEIRD)  state = KNOBSTATE_WEIRD;
    if (rflags & CXCF_FLAG_DEFUNCT)      state = KNOBSTATE_DEFUNCT;
    if (rflags & CXCF_FLAG_SFERR_MASK)   state = KNOBSTATE_SFERR;
    if (rflags & CXCF_FLAG_HWERR_MASK)   state = KNOBSTATE_HWERR;
    if (rflags & CXCF_FLAG_NOTFOUND)     state = KNOBSTATE_NOTFOUND;

    return state;
}

void        set_knobstate   (DataKnob k, knobstate_t newstate)
{
    if (newstate != k->curstate)
    {
        if (k->vmtlink != NULL  &&
            k->vmtlink->Colorize != NULL)
            k->vmtlink->Colorize(k, newstate);

        k->curstate = newstate;
    }
}

static void set_attn(DataKnob k, knobstate_t state, int on, int seconds)
{
    if (on)
    {
        k->attn_endtime = time(NULL) + seconds;
        k->attn_state   = state;
    }
    else
        k->attn_endtime = 0;
}

void        set_knob_relax  (DataKnob k, int onoff)
{
    set_attn(k, KNOBSTATE_RELAX,   onoff, KNOBS_RELAX_PERIOD);
}

void        set_knob_otherop(DataKnob k, int onoff)
{
    set_attn(k, KNOBSTATE_OTHEROP, onoff, KNOBS_OTHEROP_PERIOD);
}


static char *_datatree_knobstates_list[] =
{
    "UNINITIALIZED", "Uninitialized",
    "JustCreated",   "Just created",
    "Normal",        "Normal",
    "Yellow",        "Value in yellow zone",
    "Red",           "Value in red zone",
    "Weird",         "Value is weird",
    "Defunct",       "Defunct channel",
    "HWerr",         "Hardware error",
    "SFerr",         "Software error",
    "NOTFOUND",      "Channel not found",
    "Relax",         "Relaxing after alarm",
    "OtherOp",       "Other operator is active",
    "PrglyChg",      "Programmatically changed",
};

char *strknobstate_short(knobstate_t state)
{
  static char buf[100];

    if (state < KNOBSTATE_FIRST  ||  state > KNOBSTATE_LAST)
    {
        sprintf(buf, "?KNOBSTATE_%d?", state);
        return buf;
    }
    
    return _datatree_knobstates_list[state * 2];
}

char *strknobstate_long (knobstate_t state)
{
  static char buf[100];

    if (state < KNOBSTATE_FIRST  ||  state > KNOBSTATE_LAST)
    {
        sprintf(buf, "Unknown knob state #%d", state);
        return buf;
    }
    
    return _datatree_knobstates_list[state * 2 + 1];
}


void *find_knobs_nearest_upmethod(DataKnob k, int vmtoffset)
{
  DataKnob    holder;
  DataSubsys  subsys;
  void       *fptr;

    /* Search for nearest implementation */
    for (fptr =  NULL,     holder = (k->type == DATAKNOB_CONT  ||
                                     k->type == DATAKNOB_GRPG)? k : k->uplink,
                           subsys = holder != NULL? holder->u.c.subsyslink : NULL;
         fptr == NULL  &&  holder != NULL;
         holder =  holder->uplink)
        if (holder->vmtlink != NULL)
            fptr = *( (void **) (((int8*)(holder->vmtlink)) + vmtoffset) );

    if (fptr == NULL  &&  subsys != NULL  &&  subsys->sys_vmtlink != NULL)
        fptr = *( (void **) (((int8*)(subsys->sys_vmtlink)) + vmtoffset) );

    return fptr;
}


int  set_knob_controlvalue(DataKnob k, double      v, int fromlocal)
{
  int                   ret     = 0;
  dataknob_knob_data_t *kd      = &(k->u.k);
  dataknob_knob_vmt_t  *vmtlink = (dataknob_knob_vmt_t *)(k->vmtlink);
  double                minv, maxv;
  _k_sndphys_f          SndPhys;

    /* Is it a knob at all? */
    if (k->type != DATAKNOB_KNOB) return -1;

    if (fromlocal  &&  !k->is_rw) return -1;

    /* Fit into bounds */
    if (fromlocal)
    {
        minv = maxv = 0.0;
        if (kd->num_params > DATAKNOB_PARAM_ALWD_MIN)
            minv = kd->params[DATAKNOB_PARAM_ALWD_MIN].value;
        if (kd->num_params > DATAKNOB_PARAM_ALWD_MAX)
            maxv = kd->params[DATAKNOB_PARAM_ALWD_MAX].value;
        //fprintf(stderr, "%d, [%8.3f ... %8.3f]\n", kd->num_params, minv, maxv);

        if (minv < maxv)
        {
            if (v < minv)
            {
                if (kd->num_params > DATAKNOB_PARAM_STEP  &&
                    minv - v > kd->params[DATAKNOB_PARAM_STEP].value)
                    ret = 1;
                v = minv;
            }
            if (v > maxv)
            {
                if (kd->num_params > DATAKNOB_PARAM_STEP  &&
                    v - maxv > kd->params[DATAKNOB_PARAM_STEP].value)
                    ret = 1;
                v = maxv;
            }
        }
    }

    /* "Quant" wizardry */
    if (fromlocal)
    /* Save user-input value for future comparison */
    {
        kd->userval       = v;
        kd->userval_valid = 1;
    }
    else
    /* Check if we are still near user-input */
    {
        if (kd->userval_valid)
        {
            if (fabs(v - kd->userval) < kd->q * 0.999999)
                v = kd->userval;
            else
                kd->userval_valid = 0;
        }
    }

    /* Set the value in widget */
    if (
        (!fromlocal  ||
         (k->behaviour & DATAKNOB_B_IS_BUTTON) == 0)  &&
        vmtlink != NULL                               &&
        vmtlink->unif.type == DATAKNOB_KNOB           &&
        vmtlink->SetValue != NULL)
        vmtlink->SetValue(k, v);

    if (!k->is_rw) return ret;
    
    /* And mark as requiring to be updated */
    if (k->usertime != 0)
    {
        k->usertime = 0;
        if (editstate_hook != NULL) editstate_hook(k, 0);
    }

    /* ...but a cycle later */
    if (fromlocal  &&
        (k->behaviour & DATAKNOB_B_NO_WASJUSTSET) == 0)
        k->wasjustset = 1;
    
    /* And reset the "attention" (OTHEROP, RELAX) state, recolorizing accordingly */
    if (fromlocal)
    {
        set_attn     (k, KNOBSTATE_NONE, 0, 0);
        set_knobstate(k, choose_knobstate(k, k->currflags));
    }
    
    /* And set physical channel(s) value(s) */
    if (fromlocal)
    {
        SndPhys = find_knobs_nearest_upmethod(k, offsetof(dataknob_cont_vmt_t, SndPhys));
        if (SndPhys != NULL)
            SndPhys(k, v);
    }
    
    return ret;
}

int  set_knob_textvalue   (DataKnob k, const char *s, int fromlocal)
{
  int                   ret     = 0;
  dataknob_text_vmt_t  *vmtlink = (dataknob_text_vmt_t *)(k->vmtlink);
  _k_sndtext_f          SndText;

    /* Is it a text at all? */
    if (k->type != DATAKNOB_TEXT) return -1;

    /* Set the value in widget */
    if (
        (!fromlocal)                                   &&
        vmtlink != NULL                                &&
        vmtlink->unif.type == DATAKNOB_TEXT            &&
        vmtlink->SetText != NULL)
        vmtlink->SetText(k, s);

    if (!k->is_rw) return ret;
    
    /* And mark as requiring to be updated */
    if (k->usertime != 0)
    {
        k->usertime = 0;
        if (editstate_hook != NULL) editstate_hook(k, 0);
    }

    /* ...but a cycle later */
    if (fromlocal  &&
        (k->behaviour & DATAKNOB_B_NO_WASJUSTSET) == 0)
        k->wasjustset = 1;

    /* And reset the "attention" (OTHEROP, RELAX) state, recolorizing accordingly */
    if (fromlocal)
    {
        set_attn     (k, KNOBSTATE_NONE, 0, 0);
        set_knobstate(k, choose_knobstate(k, k->currflags));
    }
    
    /* And set physical channel(s) value(s) */
    if (fromlocal)
    {
        SndText = find_knobs_nearest_upmethod(k, offsetof(dataknob_cont_vmt_t, SndText));
        if (SndText != NULL)
            SndText(k, s);
    }
    
    return ret;
}


void show_knob_alarm      (DataKnob k, int onoff)
{
  _k_showalarm_f   ShowAlarm;

    if (k->type == DATAKNOB_KNOB                      &&
        (k->behaviour & DATAKNOB_B_IS_ALARM) != 0     &&
        k->uplink                            != NULL  &&
        (ShowAlarm =
         find_knobs_nearest_upmethod(k, offsetof(dataknob_cont_vmt_t, ShowAlarm))
        ) != NULL)
        ShowAlarm(k->uplink, onoff);

  set_knob_relax(k, !onoff);
}

void ack_knob_alarm       (DataKnob k)
{
  _k_ackalarm_f    AckAlarm;

    if (k->type == DATAKNOB_KNOB                            &&
        (k->behaviour & DATAKNOB_B_IS_ALARM)   != 0         &&
        (((int)(k->u.k.curv)) & CX_VALUE_LIT_MASK) != 0     &&
        k->uplink                                  != NULL  &&
        (AckAlarm =
         find_knobs_nearest_upmethod(k, offsetof(dataknob_cont_vmt_t, AckAlarm))
        ) != NULL)
        AckAlarm(k->uplink);
}

void show_knob_props      (DataKnob k)
{
  _k_showprops_f   ShowProps;
  
    if (k->type == DATAKNOB_KNOB &&
        (ShowProps =
         find_knobs_nearest_upmethod(k, offsetof(dataknob_cont_vmt_t, ShowProps))
        ) != NULL)
        ShowProps(k);
}

void show_knob_bigval     (DataKnob k)
{
  _k_showbigval_f  ShowBigVal;

    if (k->type == DATAKNOB_KNOB &&
        (ShowBigVal =
         find_knobs_nearest_upmethod(k, offsetof(dataknob_cont_vmt_t, ShowBigVal))
        ) != NULL)
        ShowBigVal(k);
}

void show_knob_histplot   (DataKnob k)
{
  _k_tohistplot_f  ToHistPlot;

    if (k->type == DATAKNOB_KNOB &&
        (ToHistPlot =
         find_knobs_nearest_upmethod(k, offsetof(dataknob_cont_vmt_t, ToHistPlot))
        ) != NULL)
        ToHistPlot(k);
}

void express_hist_interest(DataKnob k,
                           _m_histplot_update proc, void *privptr,
                           int set1_fgt0)
{
  _k_histintrst_f  HistInterest;

    if (k != NULL  &&
        (HistInterest =
         find_knobs_nearest_upmethod(k, offsetof(dataknob_cont_vmt_t, HistInterest))
        ) != NULL)
        HistInterest(k, proc, privptr, set1_fgt0);
}

int  historize_knob       (DataKnob k)
{
  _k_tohistplot_f  ToHistPlot;
  _k_historize_f   Historize;

    if (k->type == DATAKNOB_KNOB &&
        (Historize =
         find_knobs_nearest_upmethod(k, offsetof(dataknob_cont_vmt_t, Historize))
        ) != NULL)
        return Historize(k);
    else
        return -1;
}



void user_began_knob_editing(DataKnob k)
{
    k->usertime = time(NULL);
    if (editstate_hook != NULL) editstate_hook(k, 1);
}

void cancel_knob_editing    (DataKnob k)
{
    if (k->usertime != 0)
    {
        if      (k->type == DATAKNOB_KNOB)
        {
            set_knob_controlvalue(k,
                                  k->u.k.userval_valid? k->u.k.userval
                                                      : k->u.k.curv,
                                  0);
        }
        else if (k->type == DATAKNOB_TEXT)
        {
            if (editstate_hook != NULL) editstate_hook(k, 0);
        }
    }
}


void store_knob_undo_value  (DataKnob k)
{
    if (k->type != DATAKNOB_KNOB) return;

    k->u.k.undo_val       = k->u.k.userval_valid? k->u.k.userval
                                                : k->u.k.curv;
    k->u.k.undo_val_saved = 1;
}

void perform_knob_undo      (DataKnob k)
{
    if (k->type != DATAKNOB_KNOB) return;

    if (k->u.k.undo_val_saved)
        set_knob_controlvalue(k, k->u.k.undo_val, 1);
}


int  get_knob_label(DataKnob k, char **label_p)
{
    *label_p = k->label;
    
    if (*label_p != NULL  &&  **label_p == CX_KNOB_NOLABELTIP_PFX_C)
        *label_p = NULL;
    
    return *label_p != NULL  &&  **label_p != '\0';
}

int  get_knob_tip  (DataKnob k, char **tip_p)
{
    *tip_p   = k->tip;

    if (*tip_p   != NULL  &&  **tip_p   == CX_KNOB_NOLABELTIP_PFX_C)
        *tip_p   = NULL;
    
    return *tip_p   != NULL  &&  **tip_p   != '\0';
}


void get_knob_rowcol_label_and_tip(const char *ms, int n,
                                   DataKnob    model_k,
                                   char       *lbuf, size_t lbufsize,
                                   char       *tbuf, size_t tbufsize,
                                   char      **label_p,
                                   char      **tip_p)
{
  char *p;
  char *lp;
  char *tp;
    
    if (ms != NULL) extractstring(ms, n, lbuf, lbufsize);
    else            lbuf[0] = '\0';

    if ((p = strchr(lbuf, MULTISTRING_OPTION_SEPARATOR)) != NULL)
    {
        *p = '\0';
        strzcpy(tbuf, p + 1, tbufsize);
    }
    else
    {
        tbuf[0] = '\0';
    }
    
    lp = (*lbuf != '\0'  ||  model_k == NULL)? lbuf : model_k->label;
    tp = (*tbuf != '\0'  ||  model_k == NULL)? tbuf : model_k->tip;

    if (lp != NULL  &&  strcmp(lp, "!-!") == 0) lp = NULL;
    if (tp != NULL  &&  strcmp(tp, "!-!") == 0) tp = NULL;
    
    if (lp != NULL  &&  *lp == CX_KNOB_NOLABELTIP_PFX_C) lp++;
    if (tp != NULL  &&  *tp == CX_KNOB_NOLABELTIP_PFX_C) tp++;
    
    if (lp == NULL) lp = "";
    if (tp == NULL) tp = "";
    
    *label_p = lp;
    *tip_p   = tp;
}

enum
{
    LIST_NONE = 0,
    LIST_LIST,
    LIST_FMT_INT,
    LIST_FMT_FLOAT
};

int  get_knob_items_count(const char *items, datatree_items_prv_t *r)
{
  const char *p = items;
  char       *err;

#define CHECK_FOR_ERROR(condition, format_str, args...)  \
    if (condition)                                       \
    {                                                    \
        fprintf(stderr, format_str "\n", ## args);       \
        return -1;                                       \
    }

    bzero(r, sizeof(*r));
    if (items == NULL  ||  *items == '\0') return 0;

    if (*p == '#')
    {
        p++;
        if      (toupper(*p) == 'T')
            p++;
        else if (toupper(*p) == 'F')
        {
            p++;
            /* Get the list size */
            r->count = strtoul(p, &err, 10);
            CHECK_FOR_ERROR(err == p  ||  r->count == 0,
                            "missing list size at #F");

            /* Get the list format */
            switch (tolower(*err))
            {
                case 'd': case 'i': case 'o': case 'u': case 'x':
                    r->type = LIST_FMT_INT;
                    break;
                    
                case 'e': case 'f': case 'g': case 'a':
                    r->type = LIST_FMT_FLOAT;
                    break;
                    
                default:
                    CHECK_FOR_ERROR(1, "invalid list format char '%c' at #F", *err);
            }
            p = err + 1;

            /* Get the MULTIPLIER */
            CHECK_FOR_ERROR(*p++ != ',', "missing ',' after number");
            r->multiplier = strtod(p, &err);
            CHECK_FOR_ERROR(err == p, "missing MULTIPLIER");
            p = err;

            /* Get the OFFSET */
            CHECK_FOR_ERROR(*p++ != ',', "missing ',' after MULTIPLIER");
            r->offset = strtod(p, &err);
            CHECK_FOR_ERROR(err == p, "missing OFFSET");
            p = err;

            /* Okay, the rest must be format... */
            CHECK_FOR_ERROR(*p++ != ',', "missing ',' after OFFSET");
            strzcpy(r->format, p, sizeof(r->format));
        }
        else
            return -1;
    }

    if (r->type == LIST_NONE)
    {
        r->type   = LIST_LIST;
        r->count  = countstrings(p);
        r->data_p = p;
    }

    return r->count;
}

int  get_knob_item_n     (datatree_items_prv_t *r,
                          int n, char *buf, size_t bufsize,
                          char **tip_p, char **style_p)
{
  char *p;

    if (tip_p   != NULL) *tip_p   = NULL;
    if (style_p != NULL) *style_p = NULL;
  
    switch (r->type)
    {
        case LIST_LIST:
            extractstring(r->data_p, n, buf, bufsize);
            break;

        case LIST_FMT_INT:
            snprintf(buf, bufsize, r->format,
                     (int)(r->offset + n * r->multiplier));
            break;
            
        case LIST_FMT_FLOAT:
            snprintf(buf, bufsize, r->format,
                     (r->offset + n * r->multiplier));
            break;

        default:
            snprintf(buf, bufsize, r->format,
                     "???ITEM#%d???", n + 1);
    }

    if ((p = strchr(buf, MULTISTRING_OPTION_SEPARATOR)) != NULL)
    {
        *p = '\0';
        p++;
        if (tip_p != NULL) *tip_p = p;

        if ((p = strchr(p, MULTISTRING_OPTION_SEPARATOR)) != NULL)
        {
            *p = '\0';
            p++;
            if (style_p != NULL) *style_p = p;
        }
    }
    
    return 0;
}

DataSubsys get_knob_subsys(DataKnob k)
{
    if (k->type == DATAKNOB_GRPG  ||
        k->type == DATAKNOB_CONT) return k->u.c.subsyslink;
    else if (k->uplink != NULL)   return k->uplink->u.c.subsyslink;
    else                          return NULL;
}

DataKnob datatree_find_node(DataKnob current, const char *name, int rqd_type)
{
  const char *beg_p;
  const char *end_p;
  size_t      len;
  int         rest;
  DataKnob    kq;    // "Knob in Question"

    if (current == NULL  ||  name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    ////fprintf(stderr, "current=%p %d '%s' ->%p\n", current, current->type, current->ident, current->uplink);

    if (current->uplink != NULL) current = current->uplink;

    if (current->type != DATAKNOB_CONT  &&
        current->type != DATAKNOB_GRPG)
    {
        ////fprintf(stderr, "ENOTDIR %d %s\n", current->type, current->ident);
        errno = ENOTDIR;
        return NULL;
    }

    beg_p = name;
    /* Absolute path ("/..." alike)?
       Note: no need to "beg_p++", since '.' will be skipped later, along with other '.'s */
    if (*beg_p == '.')
        while (current->uplink != NULL) current = current->uplink;

    while (1)
    {
        /* Chop '.'s  */
        while (*beg_p == ':'  ||  *beg_p == '.')
        {
            if (*beg_p == ':'  &&  current->uplink != NULL)
                current = current->uplink;
            beg_p++;
        }

        /* Find end of segment */
        end_p = beg_p;
        while (*end_p != '\0'  &&
               *end_p != ':'  &&  *end_p != '.')
            end_p++;
        len = end_p - beg_p;

        /* Okay, where are we? */

        /* Have we reached end of name? */
        if (len == 0) break;

        /* Find item inside current node */
        for (rest = current->u.c.count,  kq = current->u.c.content;
             rest > 0;
             rest--,                     kq++)
        {
            if (kq->ident != NULL  &&
               cx_strmemcasecmp(kq->ident, beg_p, len) == 0) break;
        }
        if (rest <= 0)
        {
            ////fprintf(stderr, "%s notfound '%*s' in %s\n", __FUNCTION__, len, beg_p, current->ident);
            errno = ENOENT;
            return NULL;
        }

        /* Well, what have we found? */
        if (*end_p != '\0'  &&
            current->type != DATAKNOB_CONT  &&
            current->type != DATAKNOB_GRPG)
        {
            ////fprintf(stderr, "ENOTDIR2 %p %d %s (%s\n", current, current->type, current->ident, end_p);
            errno = ENOTDIR;
            return NULL;
        }

        current = kq;
        beg_p = end_p;
    }

    /* Check type, if requested */
    if (rqd_type >= 0  &&  current->type != rqd_type)
    {
        if      (rqd_type      == DATAKNOB_CONT  ||  rqd_type      == DATAKNOB_GRPG)
            errno = ENOTDIR;
        else if (current->type == DATAKNOB_CONT  ||  current->type == DATAKNOB_GRPG)
            errno = EISDIR;
        else
            errno = EINVAL;
        return NULL;
    }

    return current;
}
