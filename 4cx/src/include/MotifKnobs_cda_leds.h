#ifndef __MOTIFKNOBS_CDA_LEDS_H
#define __MOTIFKNOBS_CDA_LEDS_H


#include <X11/Intrinsic.h>

#include "cda.h"


enum
{
    MOTIFKNOBS_LEDS_PARENT_UNKNOWN = 0,
    MOTIFKNOBS_LEDS_PARENT_GRID    = 1,
};


typedef struct
{
    Widget  w;
    int     status;
} MotifKnobs_oneled_t;

typedef struct
{
    cda_context_t        cid;
    Widget               parent;
    int                  parent_kind;
    int                  size;

    MotifKnobs_oneled_t *leds;
    int                  count;
} MotifKnobs_leds_t;


int MotifKnobs_oneled_create    (MotifKnobs_oneled_t *led,
                                 Widget  parent, int size,
                                 const char *srvname);
int MotifKnobs_oneled_set_status(MotifKnobs_oneled_t *led, int status);

int MotifKnobs_leds_create      (MotifKnobs_leds_t *leds,
                                 Widget  parent, int size,
                                 cda_context_t cid, int parent_kind);
int MotifKnobs_leds_grow        (MotifKnobs_leds_t *leds);
int MotifKnobs_leds_update      (MotifKnobs_leds_t *leds);


#endif /* __MOTIFKNOBS_CDA_LEDS_H */
