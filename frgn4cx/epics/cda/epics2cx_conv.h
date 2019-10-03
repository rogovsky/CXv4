#ifndef __EPICS2CX_CONV_H
#define __EPICS2CX_CONV_H


enum
{
    DBR_class_PLAIN = 1,
    DBR_class_STS   = 2,
    DBR_class_TIME  = 3,
    DBR_class_GR    = 4,
    DBR_class_CTRL  = 5,
};

static int DBR2cxdtype(chtype DBR_type, cxdtype_t *dtype_p)
{
  int        DBR_class;
  cxdtype_t  dtype;

    if      (dbr_type_is_plain(DBR_type)) DBR_class = DBR_class_PLAIN;
    else if (dbr_type_is_STS  (DBR_type)) DBR_class = DBR_class_STS;
    else if (dbr_type_is_TIME (DBR_type)) DBR_class = DBR_class_TIME;
    else if (dbr_type_is_GR   (DBR_type)) DBR_class = DBR_class_GR;
    else if (dbr_type_is_CTRL (DBR_type)) DBR_class = DBR_class_CTRL;
    else return -1;

    if      (dbr_type_is_STRING(DBR_type)) return -1/*dtype = CXDTYPE_TEXT*/;
    else if (dbr_type_is_SHORT (DBR_type)  ||
           /*dbr_type_is_INT() doesn't exist  */
             dbr_type_is_ENUM  (DBR_type)) dtype = CXDTYPE_INT16;
    else if (dbr_type_is_FLOAT (DBR_type)) dtype = CXDTYPE_SINGLE;
    else if (dbr_type_is_CHAR  (DBR_type)) dtype = CXDTYPE_UINT8;
    else if (dbr_type_is_LONG  (DBR_type)) dtype = CXDTYPE_INT32;
    else if (dbr_type_is_DOUBLE(DBR_type)) dtype = CXDTYPE_DOUBLE;
    else return -1;

    if (dtype_p != NULL) *dtype_p = dtype;
    return DBR_class;
}

static chtype cxdtype2DBR(cxdtype_t dtype, int DBR_class)
{
  int base;

    switch (DBR_class)
    {
        case DBR_class_PLAIN: base = 0;                            break;
        case DBR_class_STS:   base = DBR_STS_STRING  - DBR_STRING; break;
        case DBR_class_TIME:  base = DBR_TIME_STRING - DBR_STRING; break;
        case DBR_class_GR:    base = DBR_GR_STRING   - DBR_STRING; break;
        case DBR_class_CTRL:  base = DBR_CTRL_STRING - DBR_STRING; break;
        default: return -1;
    }

    switch (dtype)
    {
        case CXDTYPE_INT8:   case CXDTYPE_UINT8:  return base + DBR_CHAR;
        case CXDTYPE_INT16:  case CXDTYPE_UINT16: return base + DBR_SHORT;
        case CXDTYPE_INT32:  case CXDTYPE_UINT32: return base + DBR_LONG;
        case CXDTYPE_INT64:  case CXDTYPE_UINT64: return -1;
        case CXDTYPE_SINGLE:                      return base + DBR_FLOAT;
        case CXDTYPE_DOUBLE:                      return base + DBR_DOUBLE;
        case CXDTYPE_TEXT:                        return -1;
        case CXDTYPE_UCTEXT:                      return -1;
        default: return -1;
    }
}

static rflags_t alarm2rflags_table[] =
{
    [READ_ALARM]          = 0,
    [WRITE_ALARM]         = 0,
    [HIHI_ALARM]          = CXCF_FLAG_COLOR_RED,
    [HIGH_ALARM]          = CXCF_FLAG_COLOR_YELLOW,
    [LOLO_ALARM]          = CXCF_FLAG_COLOR_RED,
    [LOW_ALARM]           = CXCF_FLAG_COLOR_YELLOW,
    [STATE_ALARM]         = 0,
    [COS_ALARM]           = 0,
    [COMM_ALARM]          = CXRF_REM_C_PROBL,//???
    [TIMEOUT_ALARM]       = CXRF_IO_TIMEOUT,//???
    [HW_LIMIT_ALARM]      = 0,
    [CALC_ALARM]          = 0,
    [SCAN_ALARM]          = 0,
    [LINK_ALARM]          = 0,
    [SOFT_ALARM]          = 0,
    [BAD_SUB_ALARM]       = 0,
    [UDF_ALARM]           = 0,
    [DISABLE_ALARM]       = 0,
    [SIMM_ALARM]          = 0,
    [READ_ACCESS_ALARM]   = 0,
    [WRITE_ACCESS_ALARM]  = 0,
};

static rflags_t alarm2rflags(int alarm_severity)
{
    if (alarm_severity >= 0  &&
        alarm_severity <  countof(alarm2rflags_table))
        return alarm2rflags_table[alarm_severity];
    else
        return 0;
};


#endif /* __EPICS2CX_CONV_H */
