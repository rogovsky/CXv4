#ifndef __CANKOZ_STR_DB_H
#define __CANKOZ_STR_DB_H


static char  *cankoz_devtypes[] =
{
    /*  0 */ "???0???",
    /*  1 */ "CANDAC16",
    /*  2 */ "CANADC40",
    /*  3 */ "CDAC20/CEDAC20",
    /*  4 */ "CAC208/CEAC208",
    /*  5 */ "SLIO24",
    /*  6 */ "CGVI8",
    /*  7 */ "CPKS8",
    /*  8 */ "CKVCH",
    /*  9 */ "CANIPP",
    /* 10 */ "CURVV",
    /* 11 */ "CAN-DDS",
    /* 12 */ "CAN-ADS3212",
    /* 13 */ "CAC168",
    /* 14 */ "CAN-MB3M",
    /* 15 */ "WELD01:Repkov",
    /* 16 */ "CIR8",
    /* 17 */ "CANIVA",
    /* 18 */ "CEAC51",
    /* 19 */ "CAN-PCSC8:Zabrodin",
    /* 20 */ "CEAC124",
    /* 21 */ "CGZI2",
    /* 22 */ "PANOV_UBS",
    /* 23 */ "CEAD20",
    /* 24 */ "CEAC121",
    /* 25 */ "ADC4CH:Bykov",
    /* 26 */ "CGTI:Chekavinsky",
    /* 27 */ "ADC2CH:Bykov",
    /* 28 */ "CEDIO_A",
    /* 29 */ "CEDIO_B",
    /* 30 */ "IVI:Bykov",
    /* 31 */ "YMDRV",
    /* 32 */ "SENKOV_EBC/CGVI8M", /* =0x20 */
    /* 33 */ "K5045BBS",
    /* 34 */ "CAN-BBZ",
    /* 35 */ "CAN-USHD",
    /* 36 */ "TVAC320",
    /* 37 */ "???37???",
    /* 38 */ "VV2222:Panov",
    /* 39 */ "TIMER:Tararyshkin/Zverev",
    /* 40 */ "CPS01(XFEL)",
    /* 41 */ "CREC:Chekavinsky",
    /* 42 */ "CEDIO_D(NICA)",
    /* 43 */ "code43:Zharikov(NICA)",
    /* 44 */ "SMC-CAN:Bykov/Tararyshkin",
    /* 45 */ "CEAC51A",
};

static char  *cankoz_GETDEVATTRS_reasons[] =
{
    "PowerOn",
    "Reset",
    "GetAttrs",
    "WhoAreHere",
    "WatchdogRestart",
    "BusoffRecovery"
};


#endif /* __CANKOZ_STR_DB_H */
