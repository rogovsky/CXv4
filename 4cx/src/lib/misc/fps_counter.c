#include <string.h>

#include "fps_counter.h"


void fps_init(fps_ctr_t *f)
{
    bzero(f, sizeof(*f));
}

void fps_beat(fps_ctr_t *f)
{
    f->dhz1s++;
    f->dhz5s++;

    if (f->dhz1s >= 1)
    {
        f->cur1s = f->num1s;
        f->num1s = 0;
        f->dhz1s = 0;
    }
    if (f->dhz5s >= 5)
    {
        f->cur5s = f->num5s;
        f->num5s = 0;
        f->dhz5s = 0;
    }
}

void fps_frme(fps_ctr_t *f)
{
    f->num1s++;
    f->num5s++;
}

int  fps_dfps(fps_ctr_t *f)
{
    ////fprintf(stderr, "%d %d\t %d %d\n", f->cur1s, f->cur5s, f->num1s, f->num5s);
    return (f->cur1s > 10)? f->cur1s * 10 : f->cur5s * 2;
}
