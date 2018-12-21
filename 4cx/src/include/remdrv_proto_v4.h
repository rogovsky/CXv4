#ifndef __REMDRV_PROTO_V4_H
#define __REMDRV_PROTO_V4_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cx.h"
#include "cx_version.h"


enum {REMDRV_DEFAULT_PORT = 8002};

enum
{
    REMDRV_PROTO_VERSION_MAJOR = 4,
    REMDRV_PROTO_VERSION_MINOR = 0,
    REMDRV_PROTO_VERSION = CX_ENCODE_VERSION(REMDRV_PROTO_VERSION_MAJOR,
                                             REMDRV_PROTO_VERSION_MINOR)
};


/* Note: commands are encoded in big-endian fashion,
   since CANGW and CM5307 are big-endian */
enum
{
    REMDRV_C_CONFIG = 0x43666967,  /* "Cfig" */
    REMDRV_C_SETDBG = 0x53646267,  /* "Sdbg" -- "Set DeBuG parameters" */
    REMDRV_C_DEBUGP = 0x44656267,  /* "Debg" */
    REMDRV_C_CRAZY  = 0x43727a79,  /* "Crzy" */
    REMDRV_C_RRUNDP = 0x52726450,  /* "RrdP" -- rrund problem */
    REMDRV_C_CHSTAT = 0x43685374,  /* "ChSt" -- "CHange STate" (SetDevState) */
    REMDRV_C_REREQD = 0x52727144,  /* "RrqD" -- "ReReQuest Data" */
    REMDRV_C_PING_R = 0x50696e67,  /* "Ping" -- application-ping */
    REMDRV_C_PONG_Y = 0x506f6e67,  /* "Pong" (reply) */
    REMDRV_C_READ   = 0x52656164,  /* "Read" (DRVA_READ) */
    REMDRV_C_WRITE  = 0x46656564,  /* "Feed" (DRVA_WRITE) */
    REMDRV_C_DATA   = 0x44617461,  /* "Data" -- ReturnDataSet() */
    REMDRV_C_RDS    = 0x43524473,  /* "CRDs" -- SetChanRDs() */
    REMDRV_C_FRHAGE = 0x46416765,  /* "FAge" -- SetChanFreshAge() */
    REMDRV_C_QUANT  = 0x51976e74,  /* "Qant" -- SetChanQuant() (should have been 67, not 97...) */
    REMDRV_C_RANGE  = 0x526e6765,  /* "Rnge" -- SetChanRange() */
    REMDRV_C_RTTYPE = 0x52657454,  /* "RetT" -- SetChanReturnType() */
};


typedef struct
{
    uint32  pktsize;
    uint32  command;

    /* Note: any component of var may contain at most 2 int32s */
    union
    {
        struct
        {
            uint32  businfocount;
            uint32  proto_version;
        } config;
        struct
        {
            int32   verblevel;
            uint32  mask;
        } setdbg;
        struct
        {
            uint32  level;
        } debugp;
        struct
        {
            int32   state;
            uint32  rflags_to_set;
        } chstat;
        struct
        {
            int32   first;
            int32   count;
        } group;
    }       var;
    
    int32   data[0];
} remdrv_pkt_header_t;

typedef struct
{
    float64 phys_r;
    float64 phys_d;
} remdrv_data_set_rds_t;

typedef struct
{
    uint32  age_nsec;
    uint32  age_sec_lo32;
    uint32  age_sec_hi32;
    uint32  reserved;
} remdrv_data_set_fresh_age_t;

typedef struct
{
    int32  q_dtype;
    int32  padding;
    uint8  q_data[8]; // !!! sizeof(CxAnyVal_t)
} remdrv_data_set_quant_t;

typedef struct
{
    int32  range_dtype;
    int32  padding;
    uint8  range_min[8]; // !!! sizeof(CxAnyVal_t)
    uint8  range_max[8]; // !!! sizeof(CxAnyVal_t)
} remdrv_data_set_range_t;

typedef struct
{
    int32   return_type;
    int32   reserved;
} remdrv_data_set_return_type_t;

typedef struct
{
} remdrv_data__t;

enum
{
    REMDRV_MAXDATASIZE = 4 * 1048576,
    REMDRV_MAXPKTSIZE  = sizeof(remdrv_pkt_header_t) + REMDRV_MAXDATASIZE,
};


static inline size_t REMDRV_PROTO_SIZE_CEIL(size_t size)
{
    return (size + 7) &~7U;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __REMDRV_PROTO_V4_H */
