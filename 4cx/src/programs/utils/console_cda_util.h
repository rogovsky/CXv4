#ifndef __CONSOLE_CDA_UTIL_H
#define __CONSOLE_CDA_UTIL_H


enum
{
    EC_OK      = 0,
    EC_HELP    = 1,
    EC_USR_ERR = 2,
    EC_ERR     = 3,
};


enum
{
    UTIL_PRINT_TIME      = 1 << 0,
    UTIL_PRINT_REFN      = 1 << 1,
    UTIL_PRINT_DTYPE     = 1 << 2,
    UTIL_PRINT_NELEMS    = 1 << 3,
    UTIL_PRINT_NAME      = 1 << 4,
    UTIL_PRINT_RELATIVE  = 1 << 5,
    UTIL_PRINT_PARENS    = 1 << 6,
    UTIL_PRINT_QUOTES    = 1 << 7,
    UTIL_PRINT_TIMESTAMP = 1 << 8,
    UTIL_PRINT_RFLAGS    = 1 << 9,
    UTIL_PRINT_NEWLINE   = 1 << 20,
};

typedef struct
{
    const char    *spec;
    cda_dataref_t  ref;
    int            options;
    cxdtype_t      dtype;
    int            n_items;
    char           dpyfmt[20+2];

    int            rd_req;

    int            wr_req;
    int            wr_snt;
    CxAnyVal_t     val2wr;
    void          *databuf; // if ==NULL, use val2wr
    size_t         databuf_allocd;
    int            num2wr;
} util_refrec_t;

typedef void (*process_one_name_t)(const char    *argv0,
                                   const char    *name,
                                   util_refrec_t *urp,
                                   void          *privptr);


void PrintDatarefData(FILE *fp, util_refrec_t *urp, int parts);
int  ParseDatarefSpec(const char    *argv0, 
                      const char    *spec,
                      char         **endptr_p,
                      char          *namebuf, size_t namebufsize,
                      util_refrec_t *urp);
int  ParseDatarefVal (const char    *argv0,
                      const char    *start,
                      char         **endptr_p,
                      util_refrec_t *urp);

const char * ExpandName(const char         *argv0,
                        const char         *chanref,
                        util_refrec_t      *urp,
                        process_one_name_t  processer, void *privptr);

#endif /* __CONSOLE_CDA_UTIL_H */
