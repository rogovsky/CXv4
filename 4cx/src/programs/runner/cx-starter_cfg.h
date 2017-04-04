#ifndef __CX_STARTER_CFG_H
#define __CX_STARTER_CFG_H


enum
{
    SUBSYS_UNUSED = 0,
    SUBSYS_CX     = 1,
    SUBSYS_FRGN   = 2,
    SUBSYS_SEPR   = 3,
    SUBSYS_SEPR2  = 4,
    SUBSYS_NEWCOL = 5,
};

typedef struct
{
    int   in_use;
    char  name [128];
    char  label[128];
    int   kind;          // SUBSYS_NNN
    int   noprocess;
    int   ignorevals;
    char *app_name;
    char *title;
    char *comment;
    char *chaninfo;
    char *start_cmd;
    char *params;
    char *stop_cmd;
} cfgline_t;

typedef struct
{
    int   in_use;
    char *name;
    char *start_cmd;
    char *stop_cmd;
    char *user;
    char *params;
} srvparams_t;

typedef struct
{
    cfgline_t   *clns_list;
    int          clns_list_allocd;
    srvparams_t *srvs_list;
    int          srvs_list_allocd;

    int          option_noprocess;

    int          cfg_noprocess;
} cfgbase_t;
// GetCfglSlot()
GENERIC_SLOTARRAY_DECLARE(, Cfgl, cfgline_t,
                          cfgbase_t *cfg, cfgbase_t *cfg)
// GetSrvpSlot()
GENERIC_SLOTARRAY_DECLARE(, Srvp, srvparams_t,
                          cfgbase_t *cfg, cfgbase_t *cfg)


cfgbase_t  * CfgBaseCreate (int option_noprocess);
void         CfgBaseClear  (cfgbase_t *cfg);
void         CfgBaseDestroy(cfgbase_t *cfg);
int          CfgBaseMerge  (cfgbase_t *cfg, const char *argv0, const char *path);


#endif /* __CX_STARTER_CFG_H */
