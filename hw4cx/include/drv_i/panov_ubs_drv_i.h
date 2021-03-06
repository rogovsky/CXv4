#ifndef __PANOV_UBS_DRV_I_H
#define __PANOV_UBS_DRV_I_H


/* Status (ST) bits' numbers */
enum
{
    /* StarterN status bits */
    PANOV_UBS_ST_STn_HEATIN = 0,
    PANOV_UBS_ST_STn_ARCLTD = 1,
    PANOV_UBS_ST_STn_RES2   = 2,
    PANOV_UBS_ST_STn_RES3   = 3,
    PANOV_UBS_ST_STn_REBOOT = 4,
    PANOV_UBS_ST_STn_RES5   = 5,
    PANOV_UBS_ST_STn_BUSOFF = 6,
    PANOV_UBS_ST_STn_ONLINE = 7,
    
    /* Degausser status bits */
    PANOV_UBS_ST_DGS_IS_ON  = 0,
    PANOV_UBS_ST_DGS_RES1   = 1,
    PANOV_UBS_ST_DGS_RES2   = 2,
    PANOV_UBS_ST_DGS_RES3   = 3,
    PANOV_UBS_ST_DGS_REBOOT = 4,
    PANOV_UBS_ST_DGS_RES5   = 5,
    PANOV_UBS_ST_DGS_BUSOFF = 6,
    PANOV_UBS_ST_DGS_ONLINE = 7,
    
    /* UBS status bits */
    PANOV_UBS_ST_UBS_POWER  = 0,
    PANOV_UBS_ST_UBS_RES1   = 1,
    PANOV_UBS_ST_UBS_RES2   = 2,
    PANOV_UBS_ST_UBS_RES3   = 3,
    PANOV_UBS_ST_UBS_REBOOT = 4,
    PANOV_UBS_ST_UBS_EXTOFF = 5,
    PANOV_UBS_ST_UBS_BUSOFF = 6,
    PANOV_UBS_ST_UBS_ONLINE = 7,
};

/* Interlock (IL) bits' numbers */
enum
{
    /* StarterN interlock bits */
    PANOV_UBS_IL_STn_CONNECT    = 0,
    PANOV_UBS_IL_STn_HEAT_OFF   = 1,
    PANOV_UBS_IL_STn_HEAT_COLD  = 2,
    PANOV_UBS_IL_STn_HEAT_RANGE = 3,
    PANOV_UBS_IL_STn_ARC_RANGE  = 4,
    PANOV_UBS_IL_STn_ARC_FAST   = 5,
    PANOV_UBS_IL_STn_HIGH_FAST  = 6,
    PANOV_UBS_IL_STn_RES7       = 7,

    /* Degausser interlock bits */
    PANOV_UBS_IL_DGS_CONNECT    = 0,
    PANOV_UBS_IL_DGS_UDGS_OFF   = 1,
    PANOV_UBS_IL_DGS_RES2       = 2,
    PANOV_UBS_IL_DGS_UDGS_RANGE = 3,
    PANOV_UBS_IL_DGS_RES4       = 4,
    PANOV_UBS_IL_DGS_DGS_FAST   = 5,
    PANOV_UBS_IL_DGS_RES6       = 6,
    PANOV_UBS_IL_DGS_RES7       = 7,

    /* UBS interlock bits */
    PANOV_UBS_IL_UBS_ST1        = 0,
    PANOV_UBS_IL_UBS_ST2        = 1,
    PANOV_UBS_IL_UBS_DGS        = 2,
    PANOV_UBS_IL_UBS_RES3       = 3,
    PANOV_UBS_IL_UBS_RES4       = 4,
    PANOV_UBS_IL_UBS_RES5       = 5,
    PANOV_UBS_IL_UBS_RES6       = 6,
    PANOV_UBS_IL_UBS_RES7       = 7,
};

enum // w30i,r10i,w30i,r64i,w64i,r2i
{
    /* Settings */
    PANOV_UBS_CHAN_SET_n_base  = 0,  PANOV_UBS_CHAN_SET_n_count = 10,
    PANOV_UBS_CHAN_SET_ST1_IHEAT = PANOV_UBS_CHAN_SET_n_base + 0,
    ////PANOV_UBS_CHAN_SET_ST1_IARC  = PANOV_UBS_CHAN_SET_n_base + 1,
    PANOV_UBS_CHAN_SET_ST2_IHEAT = PANOV_UBS_CHAN_SET_n_base + 2,
    ////PANOV_UBS_CHAN_SET_ST2_IARC  = PANOV_UBS_CHAN_SET_n_base + 3,
    PANOV_UBS_CHAN_SET_DGS_UDGS  = PANOV_UBS_CHAN_SET_n_base + 4,
    PANOV_UBS_CHAN_SET_THT_TIME  = PANOV_UBS_CHAN_SET_n_base + 5,
    ////PANOV_UBS_CHAN_SET_ARC_TIME  = PANOV_UBS_CHAN_SET_n_base + 6,
    PANOV_UBS_CHAN_SET_MODE      = PANOV_UBS_CHAN_SET_n_base + 7,
    PANOV_UBS_CHAN_SET_UNUSED8   = PANOV_UBS_CHAN_SET_n_base + 8,
    PANOV_UBS_CHAN_SET_UNUSED9   = PANOV_UBS_CHAN_SET_n_base + 9,

    /* Calibration */
    PANOV_UBS_CHAN_CLB_n_base  = 10, PANOV_UBS_CHAN_CLB_n_count = 10,
    PANOV_UBS_CHAN_CLB_ST1_IHEAT = PANOV_UBS_CHAN_CLB_n_base + 0,
    ////PANOV_UBS_CHAN_CLB_ST1_IARC  = PANOV_UBS_CHAN_CLB_n_base + 1,
    PANOV_UBS_CHAN_CLB_ST2_IHEAT = PANOV_UBS_CHAN_CLB_n_base + 2,
    ////PANOV_UBS_CHAN_CLB_ST2_IARC  = PANOV_UBS_CHAN_CLB_n_base + 3,
    PANOV_UBS_CHAN_CLB_DGS_UDGS  = PANOV_UBS_CHAN_CLB_n_base + 4,
    // + 5 unused

    /* Limits/ranges */
    PANOV_UBS_CHAN_LIM_n_base  = 20, PANOV_UBS_CHAN_LIM_n_count = 10,
    PANOV_UBS_CHAN_MIN_ST1_IHEAT = PANOV_UBS_CHAN_LIM_n_base + 0,
    ////PANOV_UBS_CHAN_MIN_ST1_IARC  = PANOV_UBS_CHAN_LIM_n_base + 1,
    PANOV_UBS_CHAN_MIN_ST2_IHEAT = PANOV_UBS_CHAN_LIM_n_base + 2,
    ////PANOV_UBS_CHAN_MIN_ST2_IARC  = PANOV_UBS_CHAN_LIM_n_base + 3,
    PANOV_UBS_CHAN_MIN_DGS_UDGS  = PANOV_UBS_CHAN_LIM_n_base + 4,
    PANOV_UBS_CHAN_MAX_ST1_IHEAT = PANOV_UBS_CHAN_LIM_n_base + 5,
    ////PANOV_UBS_CHAN_MAX_ST1_IARC  = PANOV_UBS_CHAN_LIM_n_base + 6,
    PANOV_UBS_CHAN_MAX_ST2_IHEAT = PANOV_UBS_CHAN_LIM_n_base + 7,
    ////PANOV_UBS_CHAN_MAX_ST2_IARC  = PANOV_UBS_CHAN_LIM_n_base + 8,
    PANOV_UBS_CHAN_MAX_DGS_UDGS  = PANOV_UBS_CHAN_LIM_n_base + 9,

    /* Measurements */
    PANOV_UBS_CHAN_MES_n_base  = 30, PANOV_UBS_CHAN_MES_n_count = 10,
    PANOV_UBS_CHAN_MES_ST1_IHEAT = PANOV_UBS_CHAN_MES_n_base + 0,
    PANOV_UBS_CHAN_MES_ST1_IARC  = PANOV_UBS_CHAN_MES_n_base + 1,
    PANOV_UBS_CHAN_MES_ST2_IHEAT = PANOV_UBS_CHAN_MES_n_base + 2,
    PANOV_UBS_CHAN_MES_ST2_IARC  = PANOV_UBS_CHAN_MES_n_base + 3,
    PANOV_UBS_CHAN_MES_DGS_UDGS  = PANOV_UBS_CHAN_MES_n_base + 4,
    PANOV_UBS_CHAN_CST_ST1_IHEAT = PANOV_UBS_CHAN_MES_n_base + 5,
    ////PANOV_UBS_CHAN_CST_ST1_IARC  = PANOV_UBS_CHAN_MES_n_base + 6,
    PANOV_UBS_CHAN_CST_ST2_IHEAT = PANOV_UBS_CHAN_MES_n_base + 7,
    ////PANOV_UBS_CHAN_CST_ST2_IARC  = PANOV_UBS_CHAN_MES_n_base + 8,
    PANOV_UBS_CHAN_CST_DGS_UDGS  = PANOV_UBS_CHAN_MES_n_base + 9,

    /* Commands */
    PANOV_UBS_CHAN_CMD_n_base  = 40, PANOV_UBS_CHAN_CMD_n_count = 30,
    PANOV_UBS_CHAN_POWER_ON      = PANOV_UBS_CHAN_CMD_n_base + 0,
    PANOV_UBS_CHAN_POWER_OFF     = PANOV_UBS_CHAN_CMD_n_base + 1,
    PANOV_UBS_CHAN_ST1_HEAT_ON   = PANOV_UBS_CHAN_CMD_n_base + 2,
    PANOV_UBS_CHAN_ST1_HEAT_OFF  = PANOV_UBS_CHAN_CMD_n_base + 3,
    PANOV_UBS_CHAN_ST2_HEAT_ON   = PANOV_UBS_CHAN_CMD_n_base + 4,
    PANOV_UBS_CHAN_ST2_HEAT_OFF  = PANOV_UBS_CHAN_CMD_n_base + 5,
    PANOV_UBS_CHAN_DGS_UDGS_ON   = PANOV_UBS_CHAN_CMD_n_base + 6,
    PANOV_UBS_CHAN_DGS_UDGS_OFF  = PANOV_UBS_CHAN_CMD_n_base + 7,
    // + 2 unused
    PANOV_UBS_CHAN_REBOOT_ALL    = PANOV_UBS_CHAN_CMD_n_base + 10,
    PANOV_UBS_CHAN_REBOOT_ST1    = PANOV_UBS_CHAN_CMD_n_base + 11,
    PANOV_UBS_CHAN_REBOOT_ST2    = PANOV_UBS_CHAN_CMD_n_base + 12,
    PANOV_UBS_CHAN_REBOOT_DGS    = PANOV_UBS_CHAN_CMD_n_base + 13,
    PANOV_UBS_CHAN_REBOOT_UBS    = PANOV_UBS_CHAN_CMD_n_base + 14,
    PANOV_UBS_CHAN_REREAD_HWADDR = PANOV_UBS_CHAN_CMD_n_base + 15,
    PANOV_UBS_CHAN_SAVE_SETTINGS = PANOV_UBS_CHAN_CMD_n_base + 16,
    // + 3 unused
    PANOV_UBS_CHAN_RST_ALL_ILK   = PANOV_UBS_CHAN_CMD_n_base + 20,
    PANOV_UBS_CHAN_RST_ST1_ILK   = PANOV_UBS_CHAN_CMD_n_base + 21,
    PANOV_UBS_CHAN_RST_ST2_ILK   = PANOV_UBS_CHAN_CMD_n_base + 22,
    PANOV_UBS_CHAN_RST_DGS_ILK   = PANOV_UBS_CHAN_CMD_n_base + 23,
    PANOV_UBS_CHAN_RST_UBS_ILK   = PANOV_UBS_CHAN_CMD_n_base + 24,
    PANOV_UBS_CHAN_RST_ACCSTAT   = PANOV_UBS_CHAN_CMD_n_base + 25,
    PANOV_UBS_CHAN_POWERON_CTR   = PANOV_UBS_CHAN_CMD_n_base + 26,
    PANOV_UBS_CHAN_WATCHDOG_CTR  = PANOV_UBS_CHAN_CMD_n_base + 27,
    // + 2 unused

    /* Statuses */
    PANOV_UBS_CHAN_STAT_n_base      = 70, PANOV_UBS_CHAN_STAT_n_count = 32,
    PANOV_UBS_CHAN_STAT_ST1_HEATIN    = PANOV_UBS_CHAN_STAT_n_base +  0 + PANOV_UBS_ST_STn_HEATIN,
    PANOV_UBS_CHAN_STAT_ST1_ARCLTD    = PANOV_UBS_CHAN_STAT_n_base +  0 + PANOV_UBS_ST_STn_ARCLTD,
    PANOV_UBS_CHAN_STAT_ST1_RES2      = PANOV_UBS_CHAN_STAT_n_base +  0 + PANOV_UBS_ST_STn_RES2,
    PANOV_UBS_CHAN_STAT_ST1_RES3      = PANOV_UBS_CHAN_STAT_n_base +  0 + PANOV_UBS_ST_STn_RES3,
    PANOV_UBS_CHAN_STAT_ST1_REBOOT    = PANOV_UBS_CHAN_STAT_n_base +  0 + PANOV_UBS_ST_STn_REBOOT,
    PANOV_UBS_CHAN_STAT_ST1_RES5      = PANOV_UBS_CHAN_STAT_n_base +  0 + PANOV_UBS_ST_STn_RES5,
    PANOV_UBS_CHAN_STAT_ST1_BUSOFF    = PANOV_UBS_CHAN_STAT_n_base +  0 + PANOV_UBS_ST_STn_BUSOFF,
    PANOV_UBS_CHAN_STAT_ST1_ONLINE    = PANOV_UBS_CHAN_STAT_n_base +  0 + PANOV_UBS_ST_STn_ONLINE,
    //
    PANOV_UBS_CHAN_STAT_ST2_HEATIN    = PANOV_UBS_CHAN_STAT_n_base +  8 + PANOV_UBS_ST_STn_HEATIN,
    PANOV_UBS_CHAN_STAT_ST2_ARCLTD    = PANOV_UBS_CHAN_STAT_n_base +  8 + PANOV_UBS_ST_STn_ARCLTD,
    PANOV_UBS_CHAN_STAT_ST2_RES2      = PANOV_UBS_CHAN_STAT_n_base +  8 + PANOV_UBS_ST_STn_RES2,
    PANOV_UBS_CHAN_STAT_ST2_RES3      = PANOV_UBS_CHAN_STAT_n_base +  8 + PANOV_UBS_ST_STn_RES3,
    PANOV_UBS_CHAN_STAT_ST2_REBOOT    = PANOV_UBS_CHAN_STAT_n_base +  8 + PANOV_UBS_ST_STn_REBOOT,
    PANOV_UBS_CHAN_STAT_ST2_RES5      = PANOV_UBS_CHAN_STAT_n_base +  8 + PANOV_UBS_ST_STn_RES5,
    PANOV_UBS_CHAN_STAT_ST2_BUSOFF    = PANOV_UBS_CHAN_STAT_n_base +  8 + PANOV_UBS_ST_STn_BUSOFF,
    PANOV_UBS_CHAN_STAT_ST2_ONLINE    = PANOV_UBS_CHAN_STAT_n_base +  8 + PANOV_UBS_ST_STn_ONLINE,
    //
    PANOV_UBS_CHAN_STAT_DGS_IS_ON     = PANOV_UBS_CHAN_STAT_n_base + 16 + PANOV_UBS_ST_DGS_IS_ON,
    PANOV_UBS_CHAN_STAT_DGS_RES1      = PANOV_UBS_CHAN_STAT_n_base + 16 + PANOV_UBS_ST_DGS_RES1,
    PANOV_UBS_CHAN_STAT_DGS_RES2      = PANOV_UBS_CHAN_STAT_n_base + 16 + PANOV_UBS_ST_DGS_RES2,
    PANOV_UBS_CHAN_STAT_DGS_RES3      = PANOV_UBS_CHAN_STAT_n_base + 16 + PANOV_UBS_ST_DGS_RES3,
    PANOV_UBS_CHAN_STAT_DGS_REBOOT    = PANOV_UBS_CHAN_STAT_n_base + 16 + PANOV_UBS_ST_DGS_REBOOT,
    PANOV_UBS_CHAN_STAT_DGS_RES5      = PANOV_UBS_CHAN_STAT_n_base + 16 + PANOV_UBS_ST_DGS_RES5,
    PANOV_UBS_CHAN_STAT_DGS_BUSOFF    = PANOV_UBS_CHAN_STAT_n_base + 16 + PANOV_UBS_ST_DGS_BUSOFF,
    PANOV_UBS_CHAN_STAT_DGS_ONLINE    = PANOV_UBS_CHAN_STAT_n_base + 16 + PANOV_UBS_ST_DGS_ONLINE,
    //
    PANOV_UBS_CHAN_STAT_UBS_POWER     = PANOV_UBS_CHAN_STAT_n_base + 24 + PANOV_UBS_ST_UBS_POWER,
    PANOV_UBS_CHAN_STAT_UBS_RES1      = PANOV_UBS_CHAN_STAT_n_base + 24 + PANOV_UBS_ST_UBS_RES1,
    PANOV_UBS_CHAN_STAT_UBS_RES2      = PANOV_UBS_CHAN_STAT_n_base + 24 + PANOV_UBS_ST_UBS_RES2,
    PANOV_UBS_CHAN_STAT_UBS_RES3      = PANOV_UBS_CHAN_STAT_n_base + 24 + PANOV_UBS_ST_UBS_RES3,
    PANOV_UBS_CHAN_STAT_UBS_REBOOT    = PANOV_UBS_CHAN_STAT_n_base + 24 + PANOV_UBS_ST_UBS_REBOOT,
    PANOV_UBS_CHAN_STAT_UBS_EXTOFF    = PANOV_UBS_CHAN_STAT_n_base + 24 + PANOV_UBS_ST_UBS_EXTOFF,
    PANOV_UBS_CHAN_STAT_UBS_BUSOFF    = PANOV_UBS_CHAN_STAT_n_base + 24 + PANOV_UBS_ST_UBS_BUSOFF,
    PANOV_UBS_CHAN_STAT_UBS_ONLINE    = PANOV_UBS_CHAN_STAT_n_base + 24 + PANOV_UBS_ST_UBS_ONLINE,

    /* Interlocks */
    PANOV_UBS_CHAN_ILK_n_base       = 102, PANOV_UBS_CHAN_ILK_n_count = 32,
    PANOV_UBS_CHAN_ILK_ST1_CONNECT    = PANOV_UBS_CHAN_ILK_n_base +  0 + PANOV_UBS_IL_STn_CONNECT,
    PANOV_UBS_CHAN_ILK_ST1_HEAT_OFF   = PANOV_UBS_CHAN_ILK_n_base +  0 + PANOV_UBS_IL_STn_HEAT_OFF ,
    PANOV_UBS_CHAN_ILK_ST1_HEAT_COLD  = PANOV_UBS_CHAN_ILK_n_base +  0 + PANOV_UBS_IL_STn_HEAT_COLD,
    PANOV_UBS_CHAN_ILK_ST1_HEAT_RANGE = PANOV_UBS_CHAN_ILK_n_base +  0 + PANOV_UBS_IL_STn_HEAT_RANGE,
    PANOV_UBS_CHAN_ILK_ST1_ARC_RANGE  = PANOV_UBS_CHAN_ILK_n_base +  0 + PANOV_UBS_IL_STn_ARC_RANGE,
    PANOV_UBS_CHAN_ILK_ST1_ARC_FAST   = PANOV_UBS_CHAN_ILK_n_base +  0 + PANOV_UBS_IL_STn_ARC_FAST,
    PANOV_UBS_CHAN_ILK_ST1_HIGH_FAST  = PANOV_UBS_CHAN_ILK_n_base +  0 + PANOV_UBS_IL_STn_HIGH_FAST,
    PANOV_UBS_CHAN_ILK_ST1_RES7       = PANOV_UBS_CHAN_ILK_n_base +  0 + PANOV_UBS_IL_STn_RES7,
    //
    PANOV_UBS_CHAN_ILK_ST2_CONNECT    = PANOV_UBS_CHAN_ILK_n_base +  8 + PANOV_UBS_IL_STn_CONNECT,
    PANOV_UBS_CHAN_ILK_ST2_HEAT_OFF   = PANOV_UBS_CHAN_ILK_n_base +  8 + PANOV_UBS_IL_STn_HEAT_OFF ,
    PANOV_UBS_CHAN_ILK_ST2_HEAT_COLD  = PANOV_UBS_CHAN_ILK_n_base +  8 + PANOV_UBS_IL_STn_HEAT_COLD,
    PANOV_UBS_CHAN_ILK_ST2_HEAT_RANGE = PANOV_UBS_CHAN_ILK_n_base +  8 + PANOV_UBS_IL_STn_HEAT_RANGE,
    PANOV_UBS_CHAN_ILK_ST2_ARC_RANGE  = PANOV_UBS_CHAN_ILK_n_base +  8 + PANOV_UBS_IL_STn_ARC_RANGE,
    PANOV_UBS_CHAN_ILK_ST2_ARC_FAST   = PANOV_UBS_CHAN_ILK_n_base +  8 + PANOV_UBS_IL_STn_ARC_FAST,
    PANOV_UBS_CHAN_ILK_ST2_HIGH_FAST  = PANOV_UBS_CHAN_ILK_n_base +  8 + PANOV_UBS_IL_STn_HIGH_FAST,
    PANOV_UBS_CHAN_ILK_ST2_RES7       = PANOV_UBS_CHAN_ILK_n_base +  8 + PANOV_UBS_IL_STn_RES7,
    //
    PANOV_UBS_CHAN_ILK_DGS_CONNECT    = PANOV_UBS_CHAN_ILK_n_base + 16 + PANOV_UBS_IL_DGS_CONNECT,
    PANOV_UBS_CHAN_ILK_DGS_UDGS_OFF   = PANOV_UBS_CHAN_ILK_n_base + 16 + PANOV_UBS_IL_DGS_UDGS_OFF,
    PANOV_UBS_CHAN_ILK_DGS_RES2       = PANOV_UBS_CHAN_ILK_n_base + 16 + PANOV_UBS_IL_DGS_RES2,
    PANOV_UBS_CHAN_ILK_DGS_UDGS_RANGE = PANOV_UBS_CHAN_ILK_n_base + 16 + PANOV_UBS_IL_DGS_UDGS_RANGE,
    PANOV_UBS_CHAN_ILK_DGS_RES4       = PANOV_UBS_CHAN_ILK_n_base + 16 + PANOV_UBS_IL_DGS_RES4,
    PANOV_UBS_CHAN_ILK_DGS_DGS_FAST   = PANOV_UBS_CHAN_ILK_n_base + 16 + PANOV_UBS_IL_DGS_DGS_FAST,
    PANOV_UBS_CHAN_ILK_DGS_RES6       = PANOV_UBS_CHAN_ILK_n_base + 16 + PANOV_UBS_IL_DGS_RES6,
    PANOV_UBS_CHAN_ILK_DGS_RES7       = PANOV_UBS_CHAN_ILK_n_base + 16 + PANOV_UBS_IL_DGS_RES7,
    //
    PANOV_UBS_CHAN_ILK_UBS_ST1        = PANOV_UBS_CHAN_ILK_n_base + 24 + PANOV_UBS_IL_UBS_ST1,
    PANOV_UBS_CHAN_ILK_UBS_ST2        = PANOV_UBS_CHAN_ILK_n_base + 24 + PANOV_UBS_IL_UBS_ST2,
    PANOV_UBS_CHAN_ILK_UBS_DGS        = PANOV_UBS_CHAN_ILK_n_base + 24 + PANOV_UBS_IL_UBS_DGS,
    PANOV_UBS_CHAN_ILK_UBS_RES3       = PANOV_UBS_CHAN_ILK_n_base + 24 + PANOV_UBS_IL_UBS_RES3,
    PANOV_UBS_CHAN_ILK_UBS_RES4       = PANOV_UBS_CHAN_ILK_n_base + 24 + PANOV_UBS_IL_UBS_RES4,
    PANOV_UBS_CHAN_ILK_UBS_RES5       = PANOV_UBS_CHAN_ILK_n_base + 24 + PANOV_UBS_IL_UBS_RES5,
    PANOV_UBS_CHAN_ILK_UBS_RES6       = PANOV_UBS_CHAN_ILK_n_base + 24 + PANOV_UBS_IL_UBS_RES6,
    PANOV_UBS_CHAN_ILK_UBS_RES7       = PANOV_UBS_CHAN_ILK_n_base + 24 + PANOV_UBS_IL_UBS_RES7,

    /* Interlock masking */
    PANOV_UBS_CHAN_LKM_n_base       = 134, PANOV_UBS_CHAN_LKM_n_count = 64, // with CRMs
    PANOV_UBS_CHAN_LKM_ST1_CONNECT    = PANOV_UBS_CHAN_LKM_n_base +  0 + PANOV_UBS_IL_STn_CONNECT,
    PANOV_UBS_CHAN_LKM_ST1_HEAT_OFF   = PANOV_UBS_CHAN_LKM_n_base +  0 + PANOV_UBS_IL_STn_HEAT_OFF ,
    PANOV_UBS_CHAN_LKM_ST1_HEAT_COLD  = PANOV_UBS_CHAN_LKM_n_base +  0 + PANOV_UBS_IL_STn_HEAT_COLD,
    PANOV_UBS_CHAN_LKM_ST1_HEAT_RANGE = PANOV_UBS_CHAN_LKM_n_base +  0 + PANOV_UBS_IL_STn_HEAT_RANGE,
    PANOV_UBS_CHAN_LKM_ST1_ARC_RANGE  = PANOV_UBS_CHAN_LKM_n_base +  0 + PANOV_UBS_IL_STn_ARC_RANGE,
    PANOV_UBS_CHAN_LKM_ST1_ARC_FAST   = PANOV_UBS_CHAN_LKM_n_base +  0 + PANOV_UBS_IL_STn_ARC_FAST,
    PANOV_UBS_CHAN_LKM_ST1_HIGH_FAST  = PANOV_UBS_CHAN_LKM_n_base +  0 + PANOV_UBS_IL_STn_HIGH_FAST,
    PANOV_UBS_CHAN_LKM_ST1_RES7       = PANOV_UBS_CHAN_LKM_n_base +  0 + PANOV_UBS_IL_STn_RES7,
    //
    PANOV_UBS_CHAN_LKM_ST2_CONNECT    = PANOV_UBS_CHAN_LKM_n_base +  8 + PANOV_UBS_IL_STn_CONNECT,
    PANOV_UBS_CHAN_LKM_ST2_HEAT_OFF   = PANOV_UBS_CHAN_LKM_n_base +  8 + PANOV_UBS_IL_STn_HEAT_OFF ,
    PANOV_UBS_CHAN_LKM_ST2_HEAT_COLD  = PANOV_UBS_CHAN_LKM_n_base +  8 + PANOV_UBS_IL_STn_HEAT_COLD,
    PANOV_UBS_CHAN_LKM_ST2_HEAT_RANGE = PANOV_UBS_CHAN_LKM_n_base +  8 + PANOV_UBS_IL_STn_HEAT_RANGE,
    PANOV_UBS_CHAN_LKM_ST2_ARC_RANGE  = PANOV_UBS_CHAN_LKM_n_base +  8 + PANOV_UBS_IL_STn_ARC_RANGE,
    PANOV_UBS_CHAN_LKM_ST2_ARC_FAST   = PANOV_UBS_CHAN_LKM_n_base +  8 + PANOV_UBS_IL_STn_ARC_FAST,
    PANOV_UBS_CHAN_LKM_ST2_HIGH_FAST  = PANOV_UBS_CHAN_LKM_n_base +  8 + PANOV_UBS_IL_STn_HIGH_FAST,
    PANOV_UBS_CHAN_LKM_ST2_RES7       = PANOV_UBS_CHAN_LKM_n_base +  8 + PANOV_UBS_IL_STn_RES7,
    //
    PANOV_UBS_CHAN_LKM_DGS_CONNECT    = PANOV_UBS_CHAN_LKM_n_base + 16 + PANOV_UBS_IL_DGS_CONNECT,
    PANOV_UBS_CHAN_LKM_DGS_UDGS_OFF   = PANOV_UBS_CHAN_LKM_n_base + 16 + PANOV_UBS_IL_DGS_UDGS_OFF,
    PANOV_UBS_CHAN_LKM_DGS_RES2       = PANOV_UBS_CHAN_LKM_n_base + 16 + PANOV_UBS_IL_DGS_RES2,
    PANOV_UBS_CHAN_LKM_DGS_UDGS_RANGE = PANOV_UBS_CHAN_LKM_n_base + 16 + PANOV_UBS_IL_DGS_UDGS_RANGE,
    PANOV_UBS_CHAN_LKM_DGS_RES4       = PANOV_UBS_CHAN_LKM_n_base + 16 + PANOV_UBS_IL_DGS_RES4,
    PANOV_UBS_CHAN_LKM_DGS_DGS_FAST   = PANOV_UBS_CHAN_LKM_n_base + 16 + PANOV_UBS_IL_DGS_DGS_FAST,
    PANOV_UBS_CHAN_LKM_DGS_RES6       = PANOV_UBS_CHAN_LKM_n_base + 16 + PANOV_UBS_IL_DGS_RES6,
    PANOV_UBS_CHAN_LKM_DGS_RES7       = PANOV_UBS_CHAN_LKM_n_base + 16 + PANOV_UBS_IL_DGS_RES7,
    //
    PANOV_UBS_CHAN_LKM_UBS_ST1        = PANOV_UBS_CHAN_LKM_n_base + 24 + PANOV_UBS_IL_UBS_ST1,
    PANOV_UBS_CHAN_LKM_UBS_ST2        = PANOV_UBS_CHAN_LKM_n_base + 24 + PANOV_UBS_IL_UBS_ST2,
    PANOV_UBS_CHAN_LKM_UBS_DGS        = PANOV_UBS_CHAN_LKM_n_base + 24 + PANOV_UBS_IL_UBS_DGS,
    PANOV_UBS_CHAN_LKM_UBS_RES3       = PANOV_UBS_CHAN_LKM_n_base + 24 + PANOV_UBS_IL_UBS_RES3,
    PANOV_UBS_CHAN_LKM_UBS_RES4       = PANOV_UBS_CHAN_LKM_n_base + 24 + PANOV_UBS_IL_UBS_RES4,
    PANOV_UBS_CHAN_LKM_UBS_RES5       = PANOV_UBS_CHAN_LKM_n_base + 24 + PANOV_UBS_IL_UBS_RES5,
    PANOV_UBS_CHAN_LKM_UBS_RES6       = PANOV_UBS_CHAN_LKM_n_base + 24 + PANOV_UBS_IL_UBS_RES6,
    PANOV_UBS_CHAN_LKM_UBS_RES7       = PANOV_UBS_CHAN_LKM_n_base + 24 + PANOV_UBS_IL_UBS_RES7,

    /* "Crash" masking. NOTE: CRM channels MUST follow LKM channels */
    PANOV_UBS_CHAN_CRM_n_base       = 166,
    PANOV_UBS_CHAN_CRM_ST1_CONNECT    = PANOV_UBS_CHAN_CRM_n_base +  0 + PANOV_UBS_IL_STn_CONNECT,
    PANOV_UBS_CHAN_CRM_ST1_HEAT_OFF   = PANOV_UBS_CHAN_CRM_n_base +  0 + PANOV_UBS_IL_STn_HEAT_OFF ,
    PANOV_UBS_CHAN_CRM_ST1_HEAT_COLD  = PANOV_UBS_CHAN_CRM_n_base +  0 + PANOV_UBS_IL_STn_HEAT_COLD,
    PANOV_UBS_CHAN_CRM_ST1_HEAT_RANGE = PANOV_UBS_CHAN_CRM_n_base +  0 + PANOV_UBS_IL_STn_HEAT_RANGE,
    PANOV_UBS_CHAN_CRM_ST1_ARC_RANGE  = PANOV_UBS_CHAN_CRM_n_base +  0 + PANOV_UBS_IL_STn_ARC_RANGE,
    PANOV_UBS_CHAN_CRM_ST1_ARC_FAST   = PANOV_UBS_CHAN_CRM_n_base +  0 + PANOV_UBS_IL_STn_ARC_FAST,
    PANOV_UBS_CHAN_CRM_ST1_HIGH_FAST  = PANOV_UBS_CHAN_CRM_n_base +  0 + PANOV_UBS_IL_STn_HIGH_FAST,
    PANOV_UBS_CHAN_CRM_ST1_RES7       = PANOV_UBS_CHAN_CRM_n_base +  0 + PANOV_UBS_IL_STn_RES7,
    //
    PANOV_UBS_CHAN_CRM_ST2_CONNECT    = PANOV_UBS_CHAN_CRM_n_base +  8 + PANOV_UBS_IL_STn_CONNECT,
    PANOV_UBS_CHAN_CRM_ST2_HEAT_OFF   = PANOV_UBS_CHAN_CRM_n_base +  8 + PANOV_UBS_IL_STn_HEAT_OFF ,
    PANOV_UBS_CHAN_CRM_ST2_HEAT_COLD  = PANOV_UBS_CHAN_CRM_n_base +  8 + PANOV_UBS_IL_STn_HEAT_COLD,
    PANOV_UBS_CHAN_CRM_ST2_HEAT_RANGE = PANOV_UBS_CHAN_CRM_n_base +  8 + PANOV_UBS_IL_STn_HEAT_RANGE,
    PANOV_UBS_CHAN_CRM_ST2_ARC_RANGE  = PANOV_UBS_CHAN_CRM_n_base +  8 + PANOV_UBS_IL_STn_ARC_RANGE,
    PANOV_UBS_CHAN_CRM_ST2_ARC_FAST   = PANOV_UBS_CHAN_CRM_n_base +  8 + PANOV_UBS_IL_STn_ARC_FAST,
    PANOV_UBS_CHAN_CRM_ST2_HIGH_FAST  = PANOV_UBS_CHAN_CRM_n_base +  8 + PANOV_UBS_IL_STn_HIGH_FAST,
    PANOV_UBS_CHAN_CRM_ST2_RES7       = PANOV_UBS_CHAN_CRM_n_base +  8 + PANOV_UBS_IL_STn_RES7,
    //
    PANOV_UBS_CHAN_CRM_DGS_CONNECT    = PANOV_UBS_CHAN_CRM_n_base + 16 + PANOV_UBS_IL_DGS_CONNECT,
    PANOV_UBS_CHAN_CRM_DGS_UDGS_OFF   = PANOV_UBS_CHAN_CRM_n_base + 16 + PANOV_UBS_IL_DGS_UDGS_OFF,
    PANOV_UBS_CHAN_CRM_DGS_RES2       = PANOV_UBS_CHAN_CRM_n_base + 16 + PANOV_UBS_IL_DGS_RES2,
    PANOV_UBS_CHAN_CRM_DGS_UDGS_RANGE = PANOV_UBS_CHAN_CRM_n_base + 16 + PANOV_UBS_IL_DGS_UDGS_RANGE,
    PANOV_UBS_CHAN_CRM_DGS_RES4       = PANOV_UBS_CHAN_CRM_n_base + 16 + PANOV_UBS_IL_DGS_RES4,
    PANOV_UBS_CHAN_CRM_DGS_DGS_FAST   = PANOV_UBS_CHAN_CRM_n_base + 16 + PANOV_UBS_IL_DGS_DGS_FAST,
    PANOV_UBS_CHAN_CRM_DGS_RES6       = PANOV_UBS_CHAN_CRM_n_base + 16 + PANOV_UBS_IL_DGS_RES6,
    PANOV_UBS_CHAN_CRM_DGS_RES7       = PANOV_UBS_CHAN_CRM_n_base + 16 + PANOV_UBS_IL_DGS_RES7,
    //
    PANOV_UBS_CHAN_CRM_UBS_ST1        = PANOV_UBS_CHAN_CRM_n_base + 24 + PANOV_UBS_IL_UBS_ST1,
    PANOV_UBS_CHAN_CRM_UBS_ST2        = PANOV_UBS_CHAN_CRM_n_base + 24 + PANOV_UBS_IL_UBS_ST2,
    PANOV_UBS_CHAN_CRM_UBS_DGS        = PANOV_UBS_CHAN_CRM_n_base + 24 + PANOV_UBS_IL_UBS_DGS,
    PANOV_UBS_CHAN_CRM_UBS_RES3       = PANOV_UBS_CHAN_CRM_n_base + 24 + PANOV_UBS_IL_UBS_RES3,
    PANOV_UBS_CHAN_CRM_UBS_RES4       = PANOV_UBS_CHAN_CRM_n_base + 24 + PANOV_UBS_IL_UBS_RES4,
    PANOV_UBS_CHAN_CRM_UBS_RES5       = PANOV_UBS_CHAN_CRM_n_base + 24 + PANOV_UBS_IL_UBS_RES5,
    PANOV_UBS_CHAN_CRM_UBS_RES6       = PANOV_UBS_CHAN_CRM_n_base + 24 + PANOV_UBS_IL_UBS_RES6,
    PANOV_UBS_CHAN_CRM_UBS_RES7       = PANOV_UBS_CHAN_CRM_n_base + 24 + PANOV_UBS_IL_UBS_RES7,

    PANOV_UBS_CHAN_PAD_base           = 198, PANOV_UBS_CHAN_PAD_count = 2,
    PANOV_UBS_CHAN_HWADDR             = PANOV_UBS_CHAN_PAD_base + 0,

    PANOV_UBS_NUMCHANS = 200
};


#endif /* __PANOV_UBS_DRV_I_H */
