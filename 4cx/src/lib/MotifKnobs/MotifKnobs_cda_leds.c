#include <stdlib.h>

#include <Xm/Xm.h>
#include <Xm/ToggleB.h>

#include "misclib.h"

#include "Xh.h"

#include "MotifKnobs_cda_leds.h"


static void ledCB(Widget     w,
                  XtPointer  closure    __attribute__((unused)),
                  XtPointer  call_data  __attribute__((unused)))
{
    XtVaSetValues(w, XmNset, False, NULL);
}

static void BlockButton2Handler(Widget     w        __attribute__((unused)),
                                XtPointer  closure  __attribute__((unused)),
                                XEvent    *event,
                                Boolean   *continue_to_dispatch)
{
  XButtonEvent *ev  = (XButtonEvent *)event;
  
    if (ev->type == ButtonPress  &&  ev->button == Button2)
        *continue_to_dispatch = False;
}

int MotifKnobs_oneled_create    (MotifKnobs_oneled_t *led,
                                 Widget  parent, int size,
                                 const char *srvname)
{
  CxWidget     w;
  XmString     s;
  int          no_decor = 0;

    if (size < 0)
    {
        size = -size;
        no_decor = 1;
    }
    if (size <= 1) size = 15;

    w = XtVaCreateManagedWidget("LED",
                                xmToggleButtonWidgetClass,
                                parent,
                                XmNtraversalOn,    False,
                                XmNlabelString,    s = XmStringCreateSimple(""),
                                XmNindicatorSize,  abs(size),
                                XmNspacing,        0,
                                XmNindicatorOn,    XmINDICATOR_CROSS,
                                XmNindicatorType,  XmONE_OF_MANY_ROUND,
                                XmNset,            False,
                                NULL);
    XmStringFree(s);
    if (no_decor)
        XtVaSetValues(w,
                      XmNmarginWidth,  0,
                      XmNmarginHeight, 0,
                      NULL);

    XhSetBalloon     (w, srvname);
    XtAddCallback    (w, XmNvalueChangedCallback, ledCB, NULL);
    XtAddEventHandler(w, ButtonPressMask, False, BlockButton2Handler, NULL); // Temporary countermeasure against Motif Bug#1117

    led->w      = w;
    led->status = CDA_SERVERSTATUS_ERROR;

    return 0;
}

int MotifKnobs_oneled_set_status(MotifKnobs_oneled_t *led, int status)
{
  int  idx;
    
    idx                                              = XH_COLOR_JUST_GREEN;
    if (status == CDA_SERVERSTATUS_FROZEN)       idx = XH_COLOR_BG_DEFUNCT;
    if (status == CDA_SERVERSTATUS_ALMOSTREADY)  idx = XH_COLOR_JUST_YELLOW;
    if (status == CDA_SERVERSTATUS_DISCONNECTED) idx = XH_COLOR_JUST_RED;
    
    XtVaSetValues(led->w, XmNunselectColor, XhGetColor(idx), NULL);
    
    led->status = status;

    return 0;
}

int MotifKnobs_leds_create      (MotifKnobs_leds_t *leds,
                                 Widget  parent, int size,
                                 cda_context_t cid, int parent_kind)
{
    bzero(leds, sizeof(*leds));
    leds->cid         = cid;
    leds->parent      = parent;
    leds->parent_kind = parent_kind;
    leds->size        = size;

    return MotifKnobs_leds_grow(leds);
}

int MotifKnobs_leds_grow        (MotifKnobs_leds_t *leds)
{
  int  old_count = leds->count;
  int  new_count = cda_status_srvs_count(leds->cid);
  int  n;

    if (new_count < 0) return -1;
    if (new_count <= leds->count) return 0;
    if (GrowUnitsBuf(&(leds->leds),
                     &(leds->count), new_count, sizeof(leds->leds[0])) < 0)
        return -1;

    if (leds->parent_kind == MOTIFKNOBS_LEDS_PARENT_GRID)
        XhGridSetSize(leds->parent, new_count, 1);
    for (n = old_count;  n < new_count;  n++)
    {
        MotifKnobs_oneled_create    (leds->leds + n,
                                     leds->parent, leds->size,
                                     cda_status_srv_name(leds->cid, n));
        MotifKnobs_oneled_set_status(leds->leds + n,
                                     cda_status_of_srv(leds->cid, n));
        if (leds->parent_kind == MOTIFKNOBS_LEDS_PARENT_GRID)
            XhGridSetChildAlignment(leds->leds[n].w, 0, 0);
    }

    return 0;
}

int MotifKnobs_leds_update      (MotifKnobs_leds_t *leds)
{
  int  n;

    for (n = 0;  n < leds->count;  n++)
        MotifKnobs_oneled_set_status(leds->leds + n,
                                     cda_status_of_srv(leds->cid, n));

    return 0;
}
