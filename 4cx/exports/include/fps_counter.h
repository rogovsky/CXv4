#ifndef __FPS_COUNTER_H
#define __FPS_COUNTER_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


typedef struct
{
    int  num1s;
    int  num5s;

    int  dhz1s;
    int  dhz5s;

    int  cur1s;
    int  cur5s;
} fps_ctr_t;


void fps_init(fps_ctr_t *f);
void fps_beat(fps_ctr_t *f);
void fps_frme(fps_ctr_t *f);
int  fps_dfps(fps_ctr_t *f);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __FPS_COUNTER_H */
