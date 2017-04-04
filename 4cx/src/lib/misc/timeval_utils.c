#include "timeval_utils.h"


/*
 *  timeval_subtract
 *
   Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.

 *  Note:
 *      Copied from the '(libc.info.gz)High-Resolution Calendar'.
 *      A small change: copy *y to yc.
 */

int timeval_subtract  (struct timeval *result,
                       struct timeval *x, struct timeval *y)
{
  struct timeval yc = *y; // A copy of y -- to preserve y from garbling
    
    /* Perform the carry for the later subtraction by updating Y. */
    if (x->tv_usec < yc.tv_usec) {
      int nsec = (yc.tv_usec - x->tv_usec) / 1000000 + 1;
        yc.tv_usec -= 1000000 * nsec;
        yc.tv_sec += nsec;
    }
    if (x->tv_usec - yc.tv_usec > 1000000) {
      int nsec = (yc.tv_usec - x->tv_usec) / 1000000;
        yc.tv_usec += 1000000 * nsec;
        yc.tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
     `tv_usec' is certainly positive. */
    result->tv_sec = x->tv_sec - yc.tv_sec;
    result->tv_usec = x->tv_usec - yc.tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < yc.tv_sec;
}

/*
 *  timeval_add
 *      Sums the `struct timeval' values, result=x+y.
 *
 *  Note:
 *      It is safe for `result' and `x'-or-`y' to be the same var.
 */

void timeval_add      (struct timeval *result,
                       struct timeval *x, struct timeval *y)
{
    result->tv_sec  = x->tv_sec  + y->tv_sec;
    result->tv_usec = x->tv_usec + y->tv_usec;
    /* Perform carry in case of usec overflow; may be done unconditionally */
    if (result->tv_usec >= 1000000)
    {
        result->tv_sec  += result->tv_usec / 1000000;
        result->tv_usec %= 1000000;
    }
}

void timeval_add_usecs(struct timeval *result,
                       struct timeval *x, int usecs_to_add)
{
  struct timeval arg2;
  
    arg2.tv_sec  = 0;
    arg2.tv_usec = usecs_to_add;
    timeval_add(result, x, &arg2);
}
