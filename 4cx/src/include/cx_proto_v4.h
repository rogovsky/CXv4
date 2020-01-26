#ifndef __CX_PROTO_V4_H
#define __CX_PROTO_V4_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cx.h"
#include "cx_version.h"


enum {  CX_V4_INET_PORT     = 8012 };
#define CX_V4_UNIX_ADDR_FMT "/tmp/cxv4-%d-socket"
enum {  CX_V4_INET_RESOLVER = CX_V4_INET_PORT};
#define CX_V4_UNIX_RESOLVER "/tmp/cxv4-r-socket"

enum
{
    CX_V4_PROTO_VERSION_MAJOR = 4,
    CX_V4_PROTO_VERSION_MINOR = 0,
    CX_V4_PROTO_VERSION = CX_ENCODE_VERSION(CX_V4_PROTO_VERSION_MAJOR,
                                            CX_V4_PROTO_VERSION_MINOR)
};


/* ==== Packet header stuff ======================================= */

typedef struct
{
    uint32  DataSize;   // Size of data (w/o header!) in bytes
    uint32  Type;       // Packet type
    uint32  ID;         // Connection identifier (assigned by server)
    uint32  Seq;        // Packet seq. number (client-side)
    int32   NumChunks;  // # of data chunks
    uint32  Status;     // Status of operation (in response packets)
    uint32  var1;       // Type-dependent #1; =0x12345678 in CONNECT packets
    uint32  var2;       // Type-dependent #2; =CX_V4_PROTO_VERSION in CONNECT packets
    uint8   data[0];    // Begin of the data
} CxV4Header;

enum
{
    CX_V4_MAX_DATASIZE = 16 * 1048576, /* 16M should be enough for all CS applications as of 23.06.2016 */
    CX_V4_MAX_PKTSIZE  = CX_V4_MAX_DATASIZE + sizeof(CxV4Header),

    /* 1500 -- Ethernet MTU; 28=20+8 -- IP and UDP headers */
    CX_V4_MAX_UDP_DATASIZE = 1500 - 28 - sizeof(CxV4Header),
    CX_V4_MAX_UDP_PKTSIZE  = CX_V4_MAX_UDP_DATASIZE + sizeof(CxV4Header),
};

/*** CXT4_nnn -- possible values of CxV4Header.Type *****************/
enum
{
    /* Special endian-independent codes */
    CXT4_EBADRQC        =  1 * 0x01010101,
    CXT4_DISCONNECT     =  2 * 0x01010101,
    
    CXT4_ENDIANID       = 10 * 0x01010101,
    CXT4_LOGIN          = 11 * 0x01010101,
    CXT4_LOGOUT         = 12 * 0x01010101,
    CXT4_SEARCH         = 13 * 0x01010101,
    CXT4_SEARCH_RESULT  = 14 * 0x01010101,

    CXT4_READY          = 20 * 0x01010101,
    CXT4_ACCESS_GRANTED = 21 * 0x01010101,
    CXT4_ACCESS_DENIED  = 22 * 0x01010101,
    CXT4_DIFFPROTO      = 23 * 0x01010101,
    CXT4_TIMEOUT        = 24 * 0x01010101,
    CXT4_MANY_CLIENTS   = 25 * 0x01010101,
    CXT4_EINVAL         = 26 * 0x01010101,
    CXT4_ENOMEM         = 27 * 0x01010101,
    CXT4_EPKT2BIG       = 28 * 0x01010101,
    CXT4_EINTERNAL      = 29 * 0x01010101,
    CXT4_PING           = 30 * 0x01010101,  /* "Request" */
    CXT4_PONG           = 31 * 0x01010101,  /* "Reply" */
    CXT4_MUSTER         = 32 * 0x01010101,

    /* Regular error codes */
    CXT4_err            = 1000,
    CXT4_OK             = CXT4_err + 0,

    /* 2000-2999: unused for now */

    /* 3000-3999: also unused */

    /* 4000-4999: console services */

    /* 5000-????: data services */
    CXT4_data           = 5000,
    CXT4_END_OF_CYCLE   = CXT4_data + 0, // S>C notification
    CXT4_DATA_IO        = CXT4_data + 1,
    CXT4_DATA_MONITOR   = CXT4_data + 2,
};

enum {CXV4_VAR1_ENDIANNESS_SIG = 0x12345678};

//--------------------------------------------------------------------

enum
{
    CXC_req_char = '>',
    CXC_rpy_char = '<',
};

#define CXC_CVT_TO_RPY(opcode)  (((opcode) & 0xFFFFFF00) | CXC_rpy_char)
#define CXC_REQ_CMD(c2, c3, c4) ((uint32)(CXC_req_char | (c2 << 8) | (c3 << 16) | (c4 << 24)))
#define CXC_RPY_CMD(c2, c3, c4) ((uint32)(CXC_rpy_char | (c2 << 8) | (c3 << 16) | (c4 << 24)))

enum
{
    CXC_SEARCH  = CXC_REQ_CMD('S', 'r', 'h'), // For UDP only

    CXC_RESOLVE = CXC_REQ_CMD('R', 's', 'l'),

    CXC_SETMON  = CXC_REQ_CMD('C', 'M', 's'),
    CXC_DELMON  = CXC_REQ_CMD('C', 'M', 'd'),
    CXC_CHGMON  = CXC_REQ_CMD('C', 'M', 'c'),
    CXC_PEEK    = CXC_REQ_CMD('C', 'p', 'k'),
    CXC_RQRDC   = CXC_REQ_CMD('C', 'r', 'd'),
    CXC_RQWRC   = CXC_REQ_CMD('C', 'w', 'r'),
    CXC_NEWVAL  = CXC_RPY_CMD('C', 'v', 'l'), // S>C only
    CXC_CURVAL  = CXC_RPY_CMD('C', 'c', 'v'), // S>C only

    CXC_STRS    = CXC_REQ_CMD('P', 's', 't'),
    CXC_RDS     = CXC_REQ_CMD('P', 'r', 'd'),
    CXC_FRH_AGE = CXC_REQ_CMD('P', 'f', 'a'),
    CXC_QUANT   = CXC_REQ_CMD('P', 'q', 'a'),
    CXC_RANGE   = CXC_REQ_CMD('P', 'r', 'n'),
};

enum
{
    CX_MON_COND_ON_UPDATE = 1,
    CX_MON_COND_ON_CYCLE  = 2,
    CX_MON_COND_ON_DELTA  = 3,
};

//--------------------------------------------------------------------

/**/

typedef struct
{
    uint32  OpCode;    // Command code
    uint32  ByteSize;  // Total size of chunk in bytes
    int32   param1;
    int32   param2;
    uint32  rs1;
    uint32  rs2;
    uint32  rs3;
    uint32  rs4;
    uint8   data[0];
} CxV4Chunk;

static inline size_t CXV4_CHUNK_CEIL(size_t size)
{
    return (size + 15) &~15U;
}

/**/

typedef struct
{
    CxV4Chunk ck;
    char      progname[40];
    char      username[40];
} CxV4LoginChunk;


/**/
typedef struct
{
    CxV4Chunk  ck;
    int32      cpid;   // ControlPoint ID -- the "key"
    int32      hwid;   // HWchan ID -- what to use for read/write ops
    //??? Maybe pack rw+dtype somehow?
    int32      rw;     // 0 -- readonly, 1 -- read/write
    int32      dtype;  // cxdtype_t, with 3 high bytes of 0s
    int32      nelems; // Max # of units; ==1 for scalar channels
    //??? color and other properties?  Or use other requests for individual parameters?
    int32      zzz;
    uint32     rs1;
    uint32     rs2;
} CxV4CpointPropsChunk;

typedef struct
{
    CxV4Chunk  ck;
    int32      cpid;
    int32      dtype_and_cond; // byte0:dtype, byte1:cond
    uint8      moninfo[8];
} CxV4MonitorChunk;

typedef struct
{
    CxV4Chunk  ck;
    int32      hwid;
    uint32  rs1;
    uint32  rs2;
    uint32  rs3;
} CxV4ReadChunk;

typedef struct
{
    CxV4Chunk  ck;
    int32      hwid;
    int32      dtype;
    int32      nelems;
    int32      _padding_;
    uint8      data[0];
} CxV4WriteChunk;

typedef struct
{
    CxV4Chunk  ck;
    int32      hwid;
    int32      dtype;
    int32      nelems;
    uint32     rflags;
    uint32     timestamp_nsec;
    uint32     timestamp_sec_lo32;
    uint32     timestamp_sec_hi32;
    uint32     Seq;
    uint8      data[0];
} CxV4ResultChunk;

typedef struct
{
    CxV4Chunk  ck;
    int32      hwid;
    uint32     fresh_age_nsec;
    uint32     fresh_age_sec_lo32;
    uint32     fresh_age_sec_hi32;
} CxV4FreshAgeChunk;

typedef struct
{
    CxV4Chunk  ck;
    int32      hwid;
    int32      q_dtype;
    uint8      q_data[8]; // !!! sizeof(CxAnyVal_t)
} CxV4QuantChunk;

typedef struct
{
    CxV4Chunk  ck;
    int32      hwid;
    int32      range_dtype;
    uint8      range_min[8]; // !!! sizeof(CxAnyVal_t)
    uint8      range_max[8]; // !!! sizeof(CxAnyVal_t)
    uint8      _padding_[8]; // For sizeof(CxV4RangeChunk) to be a multiple of 16
} CxV4RangeChunk;

typedef struct
{
    CxV4Chunk  ck;
    int32      offsets[8];
    uint8      data[0];
} CxV4StrsChunk;

typedef struct
{
    CxV4Chunk  ck;
    int32      hwid;
    int32      phys_count;
    uint32  rs1;
    uint32  rs2;
    uint8      data[0];
} CxV4RDsChunk;


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CX_PROTO_V4_H */
