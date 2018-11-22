/*********************************************************************

  Notes:
      0. This is a work in progress.
      1. This "driver" uses cxsd_dbP/cxsd_hwP internals and thus is
         tightly dependent on libcxsd.
      2. Mapping is created for EACH instance.
         However, it could be beneficial to maintain a driver-wide
         (instead of per-device) table of mappings, so that when a
         previously used devtype appears again, just an existing map
         is picked, without repeating the mapping-creation work.
      3. Channel names are taken from devtype.
         While MANY names can point to a given channel number,
         only the first of them (in namespace-specified order) is taken
         to be that channel's definitive MQTT name.
      4. Full MQTT names are taken to be "DEVNAME/CHANNAME",
         with all '.'s in channel names replaced with '/'s.

*********************************************************************/

#include "cxsd_driver.h"
#include "cxsd_hwP.h"


typedef struct
{
    uint8       rw;
    cxdtype_t   dtype;
    int         max_nelems;
    const char *mqtt_name;
} chan_map_t;
typedef struct
{
    int                 devid;

    int                 numchans;
    void               *databuf;
    chan_map_t         *map;
    char               *namesbuf;
} privrec_t;


static const char *GetDevInstname(int devid)
{
  cxsd_hw_dev_t  *dev = cxsd_hw_devices + devid;

    //CHECK_SANITY_OF_DEVID(NULL);

    return CxsdDbGetStr(cxsd_hw_cur_db, dev->db_ref->instname_ofs);
}
static int mqtt_mapping_init_d(int devid, void *devptr,
                               int businfocount, int businfo[],
                               const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;

  const char     *type_name;
  int             type_nsp_id;
  CxsdDbDcNsp_t  *type_nsp;
  const char     *inst_name;
  size_t          inst_name_len;

  int             stage;
  int             nsline;
  int             prline;
  int             chan_maps_used;
  size_t          namesbuf_size;
  int             devchan_n;
  const char     *channame;
  char           *cp;

  size_t          databuf_size;

  cxsd_hw_chan_t *dev_base;

    if ((type_name = GetDevTypename(devid)) == NULL)
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "type_name==NULL!");
        return -CXRF_DRV_PROBL;
    }
    if ((type_nsp_id = CxsdDbFindNsp(cxsd_hw_cur_db, type_name)) < 0)
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "type_nsp_id<0!");
        return -CXRF_DRV_PROBL;
    }
    if ((type_nsp    = CxsdDbGetNsp (cxsd_hw_cur_db, type_nsp_id)) == NULL)
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "type_nsp==NULL!");
        return -CXRF_DRV_PROBL;
    }
    if ((inst_name = GetDevInstname(devid)) == NULL)
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "inst_name==NULL!");
        return -CXRF_DRV_PROBL;
    }
    inst_name_len = strlen(inst_name);

    me->devid    = devid;
    me->numchans = cxsd_hw_devices[devid].count;

    dev_base     = cxsd_hw_channels + cxsd_hw_devices[devid].first;

    for (stage = 0;  stage <= 1;  stage++)
    {
        for (nsline = 0, chan_maps_used = 0, namesbuf_size = 0;
             nsline < type_nsp->items_used;
             nsline++)
        {
            devchan_n = type_nsp->items[nsline].devchan_n;

            /* First check if this line's devchan_n isn't a duplicate of any previous line */
            for (prline = 0;  prline < nsline;  prline++)
                if (devchan_n == type_nsp->items[prline].devchan_n) goto NEXT_LINE;
            chan_maps_used++;

            /* Okay -- let's count this one */
            if ((channame = CxsdDbGetStr(cxsd_hw_cur_db,
                                         type_nsp->items[nsline].name_ofs)) == NULL)
            {
                DoDriverLog(devid, DRIVERLOG_ERR,
                            "unable to CxsdDbGetStr([nsline=%d].name_ofs=%d)",
                            nsline, type_nsp->items[nsline].name_ofs);
                return -CXRF_DRV_PROBL;
            }

            if (stage > 0)
            {
                cp = me->namesbuf + namesbuf_size;

                me->map[devchan_n].rw         = dev_base[devchan_n].rw;
                me->map[devchan_n].dtype      = dev_base[devchan_n].dtype;
                me->map[devchan_n].max_nelems = dev_base[devchan_n].max_nelems;
                me->map[devchan_n].mqtt_name  = cp;

                strcpy(cp, inst_name); cp += inst_name_len; *cp++ = '/';
                strcpy(cp, channame);
                /* Perform '.'->'/' translation */
                for (;  *cp != '\0'; cp++)
                    if (*cp == '.') *cp = '/';
            }

            namesbuf_size += inst_name_len + 1 + strlen(channame) + 1;

 NEXT_LINE:;
        }

        // Allocate memory at the end of stage 0
        if (stage == 0)
        {
            if (chan_maps_used == 0)
            {
                DoDriverLog(devid, DRIVERLOG_ERR, "mapping is empty, nothing to do");
                return -CXRF_CFG_PROBL;
            }

            databuf_size = sizeof(me->map[0]) * me->numchans + namesbuf_size;
            if ((me->databuf = malloc(databuf_size)) == NULL)
            {
                DoDriverLog(devid, DRIVERLOG_ERR | DRIVERLOG_ERRNO,
                            "unable to allocate %zd bytes for databuf",
                            databuf_size);
                return -CXRF_DRV_PROBL;
            }
            bzero(me->databuf, databuf_size);
            me->map      = me->databuf;
            me->namesbuf = (char *)(me->map + me->numchans);
        }
    }

#if 1
    {
      int  chn;

fprintf(stderr, "numchans=%d\n", me->numchans);
        for (chn = 0;  chn < me->numchans;  chn++)
            if (me->map[chn].mqtt_name != NULL)
                fprintf(stderr, "%d:\t%c%d:%d\t%s\n",
                        chn,
                        me->map[chn].rw? 'w' : 'r',
                        me->map[chn].dtype,
                        me->map[chn].max_nelems,
                        me->map[chn].mqtt_name);
    }
#endif

    return DEVSTATE_OPERATING;
}

static void mqtt_mapping_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    safe_free(me->databuf);  me->databuf = NULL;
}

static void mqtt_mapping_rw_p(int devid, void *devptr,
                              int action,
                              int count, int *addrs,
                              cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t      *me = (privrec_t *)devptr;

}

DEFINE_CXSD_DRIVER(mqtt_mapping, "MQTT-mapping driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   mqtt_mapping_init_d, mqtt_mapping_term_d, mqtt_mapping_rw_p);
