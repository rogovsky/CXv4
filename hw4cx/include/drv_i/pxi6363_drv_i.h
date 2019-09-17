#ifndef __PXI6363_DRV_I_H
#define __PXI6363_DRV_I_H


/* General device specs */
enum
{
    PXI6363_NUM_ADC_LINES    = 16,
    PXI6363_NUM_OUT_LINES    = 4,
    PXI6363_NUM_DIO_LINES    = 48,

    PXI6363_NUM_LINES        = PXI6363_NUM_ADC_LINES, // That's for pzframe*
    PXI6363_MAX_NUMPTS       = 10000, // A totally arbitrary limit
    PXI6363_TABLE_MAX_NSTEPS = 20000, // the same
};
#define PXI6363_MIN_VALUE -10.0
#define PXI6363_MAX_VALUE +10.0

// w30i,w20d,r30i,r20d,r100d,r16d10000,r84d,w100d,w4d20000,w96d,r100i,w100i,r100i
enum
{
    PXI6363_CONFIG_CHAN_base    = 0,
      PXI6363_CONFIG_CHAN_count   = 100,

    PXI6363_CONFIG_INT_WR_base  = PXI6363_CONFIG_CHAN_base + 0,
      PXI6363_CONFIG_INT_WR_count = 30,
    PXI6363_CONFIG_DBL_WR_base  = PXI6363_CONFIG_CHAN_base + 30,
      PXI6363_CONFIG_DBL_WR_count = 20,
    PXI6363_CONFIG_INT_RD_base  = PXI6363_CONFIG_CHAN_base + 50,
      PXI6363_CONFIG_INT_RD_count = 30,
    PXI6363_CONFIG_DBL_RD_base  = PXI6363_CONFIG_CHAN_base + 80,
      PXI6363_CONFIG_DBL_RD_count = 20,


    PXI6363_CHAN_ADC_MODE       = PXI6363_CONFIG_INT_WR_base + 0,
    PXI6363_CHAN_OUT_MODE       = PXI6363_CONFIG_INT_WR_base + 1,

    PXI6363_CHAN_no_SHOT        = PXI6363_CONFIG_INT_WR_base + 10,
    PXI6363_CHAN_STOP           = PXI6363_CONFIG_INT_WR_base + 11,
    PXI6363_CHAN_no_PTSOFS      = PXI6363_CONFIG_INT_WR_base + 12,
    PXI6363_CHAN_NUMPTS         = PXI6363_CONFIG_INT_WR_base + 13,
    PXI6363_CHAN_STARTS_SEL     = PXI6363_CONFIG_INT_WR_base + 14,
    PXI6363_CHAN_TIMING_SEL     = PXI6363_CONFIG_INT_WR_base + 15,

    PXI6363_CHAN_no_RANGEMIN    = PXI6363_CONFIG_DBL_WR_base + 0,
    PXI6363_CHAN_no_RANGEMAX    = PXI6363_CONFIG_DBL_WR_base + 1,
    PXI6363_CHAN_OSC_RATE       = PXI6363_CONFIG_DBL_WR_base + 2,

    PXI6363_CHAN_no_CUR_PTSOFS  = PXI6363_CONFIG_INT_RD_base + 10,
    PXI6363_CHAN_CUR_NUMPTS     = PXI6363_CONFIG_INT_RD_base + 11,

    PXI6363_CHAN_MARKER         = PXI6363_CONFIG_INT_RD_base + 29,

    PXI6363_CHAN_ALL_RANGEMIN   = PXI6363_CONFIG_DBL_RD_base + 0,
    PXI6363_CHAN_ALL_RANGEMAX   = PXI6363_CONFIG_DBL_RD_base + 1,
    PXI6363_CHAN_XS_PER_POINT   = PXI6363_CONFIG_DBL_RD_base + 2,

    /* 100-199: ADC */
    PXI6363_CHAN_ADC_n_base     = 100,
      PXI6363_CHAN_ADC_n_count    = PXI6363_NUM_ADC_LINES,

    /* 200-299: oscilloscope-flavor ADC channels */
    PXI6363_CHAN_OSC_n_base     = 200,
      PXI6363_CHAN_OSC_n_count    = PXI6363_NUM_ADC_LINES,

    /* 300-399: DAC */
    PXI6363_CHAN_OUT_n_base     = 300,
      PXI6363_CHAN_OUT_n_count    = PXI6363_NUM_OUT_LINES,

    /* 400-499: tables */
    PXI6363_CHAN_OUT_TAB_n_base = 400,
      PXI6363_CHAN_OUT_TAB_n_count= PXI6363_NUM_OUT_LINES,

    /* 500-599: digital input */
    PXI6363_CHAN_OUTRB_n_base   = 500,
      PXI6363_CHAN_OUTRB_n_count  = PXI6363_NUM_DIO_LINES,

    /* 600-699: digital output */
    PXI6363_CHAN_INPRB_n_base   = 600,
      PXI6363_CHAN_INPRB_n_count  = PXI6363_NUM_DIO_LINES,

    /* 700-799: digital I/O direction (per-channel) */
    PXI6363_CHAN_DIODIR_n_base  = 700,
      PXI6363_CHAN_DIODIR_n_count = PXI6363_NUM_DIO_LINES,

    PXI6363_NUMCHANS = 800
};

enum
{
    PXI6363_ADC_MODE_SCALAR = 0,
    PXI6363_ADC_MODE_OSCILL = 1,
};

enum
{
    PXI6363_OUT_MODE_SCALAR     = 0,
    PXI6363_OUT_MODE_TABLE_ONCE = 1,
    PXI6363_OUT_MODE_TABLE_LOOP = 2,
};

enum
{
    PXI6363_STARTS_INTERNAL = 0,
};

enum
{
    PXI6363_TIMING_INTERNAL = 0,
};


#endif /* __PXI6363_DRV_I_H */