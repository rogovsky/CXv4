#ifndef __CHL_BIGVALS_TYPES_H
#define __CHL_BIGVALS_TYPES_H


enum {MAX_BIGVALS = 5};

typedef struct
{
    int            initialized;
    XtIntervalId   upd_timer;

    CxWidget       box;
    int            rows_used;
    CxWidget       grid;
    CxWidget       label     [MAX_BIGVALS];
    CxWidget       val_dpy   [MAX_BIGVALS];
    CxWidget       b_up      [MAX_BIGVALS];
    CxWidget       b_dn      [MAX_BIGVALS];
    CxWidget       b_rm      [MAX_BIGVALS];
    DataKnob       target    [MAX_BIGVALS];
    knobstate_t    colstate_s[MAX_BIGVALS];
    CxPixel        deffg;
    CxPixel        defbg;
} bigvals_rec_t;


#endif /* __CHL_BIGVALS_TYPES_H */
