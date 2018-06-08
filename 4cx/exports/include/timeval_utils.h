#ifndef __TIMEVAL_UTILS_H
#define __TIMEVAL_UTILS_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <sys/time.h>


int  timeval_subtract (struct timeval *result,
                       struct timeval *x, struct timeval *y);
void timeval_add      (struct timeval *result,
                       struct timeval *x, struct timeval *y);
void timeval_add_usecs(struct timeval *result,
                       struct timeval *x, int usecs_to_add);

#define TV_IS_AFTER(a, b) \
    ((a).tv_sec > (b).tv_sec  ||  ((a).tv_sec == (b).tv_sec  &&  (a).tv_usec > (b).tv_usec))


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __TIMEVAL_UTILS_H */
