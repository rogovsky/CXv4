#ifndef __U0632_INTERNALS_H
#define __U0632_INTERNALS_H


/* Note: because u0632 is a MASSIVELY multi-channel device (30 boxes),
   we do NOT use its U0632_CHAN_-numbers but introduce local C_-numbers */
enum
{
    C_DATA,
    C_ONLINE,
    C_CUR_M,
    C_CUR_P,
    C_M,
    C_P,
};


#endif /* __U0632_INTERNALS_H */
