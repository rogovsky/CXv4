#ifndef __G0401_DRV_I_H
#define __G0401_DRV_I_H


// w40i
enum
{
    G0401_CHAN_RAW_V_n_base = 0,   G0401_CHAN_OUT_count = 8,
    G0401_CHAN_QUANT_N_base = 8,   // Note: 1quant=100ns

    G0401_CHAN_PROGSTART    = 25,
    G0401_CHAN_T_QUANT_MODE = 28,
    G0401_CHAN_BREAK_ON_CH7 = 29,

    G0401_NUMCHANS = 40
};

enum
{
    G0401_T_QUANT_MODE_12_8US = 0, // 100ns*2^(7-0)
    G0401_T_QUANT_MODE_6_4US  = 1, // 100ns*2^(7-1)
    G0401_T_QUANT_MODE_3_2US  = 2, // 100ns*2^(7-2)
    G0401_T_QUANT_MODE_1_6US  = 3, // 100ns*2^(7-3)
    G0401_T_QUANT_MODE_800NS  = 4, // 100ns*2^(7-4)
    G0401_T_QUANT_MODE_400NS  = 5, // 100ns*2^(7-5)
    G0401_T_QUANT_MODE_200NS  = 6, // 100ns*2^(7-6)
    G0401_T_QUANT_MODE_100NS  = 7, // 100ns*2^(7-7)
};


#endif /* __G0401_DRV_I_H */
