#ifndef __SLBPM_DRV_I_H
#define __SLBPM_DRV_I_H


// auxinfo: ip_address/hostname
// r4i,r1i4,r5i,r4i32,r1i128,r4i64,r1i256,w20i,r10i
enum
{
    /* 0. Data */
    // Mins/maxs
    SLBPM_CHAN_MMX0 =  0,
    SLBPM_CHAN_MMX1 =  1,
    SLBPM_CHAN_MMX2 =  2,
    SLBPM_CHAN_MMX3 =  3,
    SLBPM_CHAN_MMXA =  4,
    // Stub (5pcs)

    // Per-cycle oscillograms; 32x int32s
    SLBPM_CHAN_OSC0 = 10,
    SLBPM_CHAN_OSC1 = 11,
    SLBPM_CHAN_OSC2 = 12,
    SLBPM_CHAN_OSC3 = 13,
    SLBPM_CHAN_OSCA = 14,

    // Per-(Nav+1) buffers; (Nav+1)x int32s
    SLBPM_CHAN_BUF0 = 15,
    SLBPM_CHAN_BUF1 = 16,
    SLBPM_CHAN_BUF2 = 17,
    SLBPM_CHAN_BUF3 = 18,
    SLBPM_CHAN_BUFA = 19,

    /* 1. Config */
    SLBPM_CONFIG_CHAN_base    = 20,
      SLBPM_CONFIG_CHAN_count = 20,

    SLBPM_CHAN__RESET = SLBPM_CONFIG_CHAN_base + 0,

    SLBPM_CHAN_DELAYA = SLBPM_CONFIG_CHAN_base + 9,   // Common delay, 1bit=10ps
    SLBPM_CHAN_DELAY0 = SLBPM_CONFIG_CHAN_base + 10,  // Individual delays 0-3
    SLBPM_CHAN_DELAY1 = SLBPM_CONFIG_CHAN_base + 11,
    SLBPM_CHAN_DELAY2 = SLBPM_CONFIG_CHAN_base + 12,
    SLBPM_CHAN_DELAY3 = SLBPM_CONFIG_CHAN_base + 13,
    SLBPM_CHAN_EMINUS = SLBPM_CONFIG_CHAN_base + 14,
    SLBPM_CHAN_AMPLON = SLBPM_CONFIG_CHAN_base + 15,
    SLBPM_CHAN_NAV    = SLBPM_CONFIG_CHAN_base + 16,
    SLBPM_CHAN_BORDER = SLBPM_CONFIG_CHAN_base + 17,
    SLBPM_CHAN_SHOT   = SLBPM_CONFIG_CHAN_base + 18,
    SLBPM_CHAN_CALB_0 = SLBPM_CONFIG_CHAN_base + 19,

    /* 2. Dummy "status" (for ADC-like progs) */
    SLBPM_STATUS_CHAN_base    = 40,
      SLBPM_STATUS_CHAN_count = 10,

    SLBPM_CHAN_CONST32        = SLBPM_STATUS_CHAN_base + 0,
    SLBPM_CHAN_CUR_NAV        = SLBPM_STATUS_CHAN_base + 1,

    SLBPM_NUMCHANS            = 50
};


#endif /* __SLBPM_DRV_I_H */
