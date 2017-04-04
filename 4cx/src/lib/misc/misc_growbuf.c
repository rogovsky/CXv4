#include <string.h>
#include <errno.h>

#include "misc_macros.h"
#include "misclib.h"


/*
 *  GrowBuf
 *      Grows the buffer to be at least `newsize' large.
 *      If it is already big enough, does nothing, else realloc()s
 *      the buffer and places new values to the *bufptr and *sizeptr.
 *
 *  Args:
 *      *bufptr   pointer to the buffer
 *      *sizeptr  current buffer size
 *      newsize   requested new size
 *
 *  Result:
 *      0   success
 *      -1  failed to realloc(), `errno'=ENOMEM
 */

int GrowBuf     (void **bufptr, size_t *sizeptr,    size_t newsize)
{
  void *newbuf;

    if (*sizeptr >= newsize) return 0;

    newbuf = safe_realloc(*bufptr, newsize);
    
    if (newbuf == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    *bufptr  = newbuf;
    *sizeptr = newsize;

    return 0;
}

/*
 *  GrowUnitsBuf
 *      Similar to GrowBuf(), but operates in "units" instead of bytes.
 *      Additionally, zeroes newly added space.
 */

int GrowUnitsBuf(void **bufptr, int    *allocd_ptr, int    grow_to, size_t unit_size)
{
  void *newbuf;
  
    if (*allocd_ptr >= grow_to) return 0;
    
    newbuf = safe_realloc(*bufptr, grow_to * unit_size);

    if (newbuf == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    bzero(((char *)newbuf) + *allocd_ptr * unit_size,
          (grow_to - *allocd_ptr) * unit_size);
    
    *bufptr     = newbuf;
    *allocd_ptr = grow_to;

    return 0;
}
