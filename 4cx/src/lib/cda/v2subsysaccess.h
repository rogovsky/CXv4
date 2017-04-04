#ifndef __V2SUBSYSACCESS_H
#define __V2SUBSYSACCESS_H


enum
{
    V2SUBSYSACCESS_T_REGULAR = 0,
};

typedef struct
{
    const char    *ident;
    const char    *label;
    const char    *tip;
    const char    *comment;
    const char    *geoinfo;
    const char    *rsrvd6;
    const char    *units;
    const char    *dpyfmt;
} v2subsysaccess_strs_t;

int v2subsysaccess_resolve(const char *argv0,
                           const char *name,
                           char *srvnamebuf, size_t srvnamebufsize,
                           int *chan_n, int *color_p,
                           double *phys_r_p, double *phys_d_p, int *phys_q_p,
                           v2subsysaccess_strs_t *strs_p);


#endif /* __V2SUBSYSACCESS_H */
