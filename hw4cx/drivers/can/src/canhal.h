#ifndef __CANHAL_H
#define __CANHAL_H


enum
{
    CANHAL_OK     = +1,
    CANHAL_ZERO   =  0,
    CANHAL_ERR    = -1,
    CANHAL_BUSOFF = -2,
};

enum
{
    CANHAL_B125K = 0,
    CANHAL_B250K = 1,
    CANHAL_B500K = 2,
    CANHAL_B1M   = 3,
};


static int  canhal_open_and_setup_line(int line_n, int speed_n, char **errstr);
static void canhal_close_line         (int  fd);

static int  canhal_send_frame         (int  fd,
                                       int  id,   int  dlc,   uint8 *data);
static int  canhal_recv_frame         (int  fd,
                                       int *id_p, int *dlc_p, uint8 *data);


#endif /* __CANHAL_H */
