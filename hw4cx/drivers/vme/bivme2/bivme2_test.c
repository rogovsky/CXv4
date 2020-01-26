#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

#include "bivme2_io.h"

#include "misc_types.h"
#include "misc_macros.h"
#include "timeval_utils.h"

#include "cxscheduler.h"


// Common VME interface
enum
{
    VME_OP_NONE      = 0,
    VME_OP_READ      = 'r',
    VME_OP_WRITE     = 'w',

    VME_COND_NONZERO = '!',  // (data & mask) != 0
    VME_COND_EQUAL   = '=',  // (data & mask  == value
};

typedef struct _vme_rw_params_t_struct
{
    int     offset;
    int     units;  // 1, 2, 4
    int     count;

    void   *addr;
    int     cond;
    uint32  mask;
    uint32  value;
    int     t_op;
} vme_rw_params_t;

typedef struct _vme_devspec_t_struct
{
    int     addr_size;
    int     am;
    uint32  base_addr;
} vme_devspec_t;

typedef struct _vme_irq_info_t_struct
{
    int              n;
    int              vector;
    vme_rw_params_t  check;
    vme_rw_params_t  reset;
} vme_irq_info_t;

//////////////////////////////////////////////////////////////////////

enum
{
    PAS_COUNT     = 1 << 0,
    PAS_MASK      = 1 << 1,
    PAS_VALUE     = 1 << 2,
    PAS_VALARR    = 1 << 3,
    PAS_VALUE_RQD = 1 << 4,
    PAS_COMMA_TRM = 1 << 5,
};

static void ParseAddrSpec(const char *argv0, const char *place,
                          const char *spec, const char **end_p,
                          vme_rw_params_t *addr_p,
                          void *databuf, size_t databufsize,
                          int flags)
{
  const char   *p = spec;
  char         *errp;
  
  char          u_c;

  uint8        *p8;
  uint16       *p16;
  uint32       *p32;
    
    if (place == NULL) place = "";

    bzero(addr_p, sizeof(*addr_p));
    addr_p->addr = databuf;
  
    u_c = tolower(*p);
    if      (u_c == 'b') addr_p->units = 1;
    else if (u_c == 's') addr_p->units = 2;
    else if (u_c == 'i') addr_p->units = 4;
    /*else if (u_c == 'q') units = 8;*/
    else                         goto ERR;
    p++;
    if (*p == ':') p++;  // Optional separator, to separate words for shell

    addr_p->offset = strtol(p, &errp, 0);
    if (errp == p) goto ERR;
    p = errp;

    if (*p == '/')
    {
        if ((flags & PAS_COUNT) == 0)
        {
            fprintf(stderr, "%s: no /COUNT is allowed in %s%saddr spec \"%s\"\n",
                    argv0, place, *place != '\0'? " " : "", spec);
            exit(1);
        }
        
        p++;
        addr_p->count = strtol(p, &errp, 0);
        if (errp == p) goto ERR;
        p = errp;
        if (addr_p->count <= 0  ||
            addr_p->count > databufsize / addr_p->units)
        {
            fprintf(stderr, "%s: COUNT=%d is out of range [1-%d] in %s%saddr spec \"%s\"\n",
                    argv0,
                    addr_p->count, databufsize / addr_p->units,
                    place, *place != '\0'? " " : "", spec);
            exit(1);
        }
    }
    else
        addr_p->count = 1;

    if (*p == '@')
    {
        if ((flags & PAS_MASK) == 0)
        {
            fprintf(stderr, "%s: no @MASK is allowed in %s%saddr spec \"%s\"\n",
                    argv0, place, *place != '\0'? " " : "", spec);
            exit(1);
        }
        
        p++;
        addr_p->mask = strtol(p, &errp, 0);
        if (errp == p) goto ERR;
        p = errp;
    }
    else
        addr_p->mask = -1; /*!!! Is there any other way to set bitsize-independent all-1s? */

    if (*p == '=')
    {
        if ((flags & PAS_VALUE) == 0)
        {
            fprintf(stderr, "%s: no =VALUE is allowed in %s%saddr spec \"%s\"\n",
                    argv0, place, *place != '\0'? " " : "", spec);
            exit(1);
        }
        
        addr_p->t_op = VME_OP_WRITE;
        addr_p->cond = VME_COND_EQUAL;

        addr_p->count = 0;
        p8 = p16 = p32 = databuf;

        while (1)
        {
            p++;
            addr_p->value = strtol(p, &errp, 0);
            if (errp == p) goto ERR;
            p = errp;

            if ((flags & PAS_VALARR) != 0)
            {
                if      (addr_p->units == 1) *p8++  = addr_p->value;
                else if (addr_p->units == 2) *p16++ = addr_p->value;
                else if (addr_p->units == 4) *p32++ = addr_p->value;
                addr_p->count++;
            }

            if ((flags & PAS_COMMA_TRM) != 0) break; /* That's specifically for IRQ_CHECK,IRQ_RESET and ON_CLOSE */

            if (*p == '\0') break;
            if (*p != ',')  goto ERR;

            /* Okay, next value... */
            if ((flags & PAS_VALARR) == 0)
            {
                fprintf(stderr, "%s: multiple VALUEs in %s%saddr spec \"%s\"\n",
                        argv0, place, *place != '\0'? " " : "", spec);
                exit(1);
            }

            if (addr_p->count >= databufsize / addr_p->units)
            {
                fprintf(stderr, "%s: too many VALUEs in %s%saddr spec \"%s\"; max is %d\n",
                        argv0, place, *place != '\0'? " " : "", spec,
                        databufsize / addr_p->units);
                exit(1);
            }

            /* Everything is okay -- let's slip to next iteration */
        }
    }
    else if (*p == '+'  &&  ((flags & PAS_MASK) != 0))
    {
        addr_p->value = 0;
        addr_p->t_op  = VME_OP_NONE;
        addr_p->cond  = VME_COND_NONZERO;
        p++;
    }
    else
    {
        if ((flags & PAS_VALUE_RQD) != 0)
        {
            fprintf(stderr, "%s: =VALUE is required in %s%saddr spec \"%s\"\n",
                    argv0, place, *place != '\0'? " " : "", spec);
            exit(1);
        }
        
        addr_p->t_op = VME_OP_READ;
    }

    if (end_p != NULL) *end_p = p;

    return;
    
 ERR:
    fprintf(stderr, "%s: error in %s%saddr spec \"%s\"\n",
            argv0, place, *place != '\0'? " " : "", spec);
    exit(1);
}

static void ParseDevSpec (const char *argv0, const char *spec,
                          vme_devspec_t *dev_p)
{
  const char *p = spec;
  const char *errp;

    dev_p->addr_size = strtol(p, &errp, 0);
    if (errp == p  ||  *errp != ':') goto ERR;
    p = errp + 1;
    if (dev_p->addr_size != 16  &&
        dev_p->addr_size != 24  &&
        dev_p->addr_size != 32) // For VME64: also 40 and 64
    {
        fprintf(stderr, "%s: bad ADDRESS_MODIFIER=%d (should be one of 16,24,32) in device spec \"%s\"\n",
                argv0, dev_p->addr_size, spec);
        exit(1);
    }

    dev_p->am        = strtol(p, &errp, 0);
    if (errp == p  ||  *errp != ':') goto ERR;
    p = errp + 1;

    dev_p->base_addr = strtoul(p, &errp, 0);
    if (errp == p)                   goto ERR;
//fprintf(stderr, "%s p=<%s> base_addr=%08x\n", __FUNCTION__, p, dev_p->base_addr);
    
    return;
    
 ERR:
    fprintf(stderr, "%s: error in device spec \"%s\"\n", argv0, spec);
    exit(1);
}

//////////////////////////////////////////////////////////////////////

static int do_io(int iohandle, vme_rw_params_t *io, int addr_size)
{
  uint8           *ptr;
  int              ofs;
  int              x;
  int              r;

    for (x = 0, ptr  = io->addr,  ofs  = io->offset;
         x < io->count;
         x++,   ptr += io->units, ofs += io->units)
    {
        if (io->t_op == VME_OP_WRITE)
        {
            if      (addr_size == 16)
            {
                if      (io->units == 1)  r = bivme2_io_a16wr8 (iohandle, ofs, *((uint8 *)ptr));
                else if (io->units == 2)  r = bivme2_io_a16wr16(iohandle, ofs, *((uint16*)ptr));
                else   /*io->units == 4*/ r = bivme2_io_a16wr32(iohandle, ofs, *((uint32*)ptr));
            }
            else if (addr_size == 24)
            {
                if      (io->units == 1)  r = bivme2_io_a24wr8 (iohandle, ofs, *((uint8 *)ptr));
                else if (io->units == 2)  r = bivme2_io_a24wr16(iohandle, ofs, *((uint16*)ptr));
                else   /*io->units == 4*/ r = bivme2_io_a24wr32(iohandle, ofs, *((uint32*)ptr));
            }
            else  /* addr_size == 32 */
            {
                if      (io->units == 1)  r = bivme2_io_a32wr8 (iohandle, ofs, *((uint8 *)ptr));
                else if (io->units == 2)  r = bivme2_io_a32wr16(iohandle, ofs, *((uint16*)ptr));
                else   /*io->units == 4*/ r = bivme2_io_a32wr32(iohandle, ofs, *((uint32*)ptr));
            }
        }
        else
        {
            if      (addr_size == 16)
            {
                if      (io->units == 1)  r = bivme2_io_a16rd8 (iohandle, ofs,   (uint8 *)ptr );
                else if (io->units == 2)  r = bivme2_io_a16rd16(iohandle, ofs,   (uint16*)ptr );
                else   /*io->units == 4*/ r = bivme2_io_a16rd32(iohandle, ofs,   (uint32*)ptr );
            }
            else if (addr_size == 24)
            {
                if      (io->units == 1)  r = bivme2_io_a24rd8 (iohandle, ofs,   (uint8 *)ptr );
                else if (io->units == 2)  r = bivme2_io_a24rd16(iohandle, ofs,   (uint16*)ptr );
                else   /*io->units == 4*/ r = bivme2_io_a24rd32(iohandle, ofs,   (uint32*)ptr );
            }
            else  /* addr_size == 32 */
            {
                if      (io->units == 1)  r = bivme2_io_a32rd8 (iohandle, ofs,   (uint8 *)ptr );
                else if (io->units == 2)  r = bivme2_io_a32rd16(iohandle, ofs,   (uint16*)ptr );
                else   /*io->units == 4*/ r = bivme2_io_a32rd32(iohandle, ofs,   (uint32*)ptr );
            }
        }

        if (r != 0) return r;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////

static int              ref;

static vme_devspec_t    dev;
static vme_irq_info_t   irq;
static int              irq_specified = 0;
static int              was_irq;

static vme_rw_params_t  onclose_params[10];
static int              onclose_count = 0;

static int              option_hex_data = 0;
static int              option_quiet    = 0;
static enum
{
    TIMES_OFF,
    TIMES_REL,
    TIMES_ISO
}                       option_times    = TIMES_OFF;

static int              sl_fd_uniq     = 0;
static void            *sl_fd_privptr1 = NULL;
static int              sl_fd_fd       = -1;
static int              sl_fd_mask     = 0;
static sl_fd_proc       sl_fd_cb       = NULL;
static void            *sl_fd_privptr2 = NULL;
            

static void irq_p(int devid, void *devptr,
                  int irq_n, int vector)
{
  vme_rw_params_t  op;
  uint32           mask;

    if (irq.vector >= 0  &&  vector != irq.vector) return;

    // Check
    if (irq.check.t_op != VME_OP_NONE)
    {
        op = irq.check;
        op.addr  = &(op.value);
        op.count = 1;
        do_io(ref, &op, dev.addr_size);
        // The check itself
        if (op.t_op == VME_OP_READ)
        {
            if      (op.units == 1) mask = 0x000000FF;
            else if (op.units == 2) mask = 0x0000FFFF;
            else  /* op.units == 2*/mask = 0xFFFFFFFF;
            if (
                (op.cond == VME_COND_NONZERO  &&
                 ((op.value & mask) &  (irq.check.mask & mask)) == 0)
                ||
                (op.cond == VME_COND_EQUAL  &&
                 ((op.value & mask) != (irq.check.mask & mask)))
               ) return;
            if (!option_quiet)
            {
                printf("# IRQ; read=");
                printf(option_hex_data? "0x%x" : "%u", op.value & mask);
                printf("\n");
            }
        }
    }
    // Reset
    if (irq.reset.t_op != VME_OP_NONE)
    {
        op = irq.reset;
        op.addr  = &(op.value);
        op.count = 1;
        do_io(ref, &op, dev.addr_size);
    }

    was_irq = 1;
}

sl_fdh_t  sl_add_fd     (int uniq, void *privptr1,
                         int fd, int mask, sl_fd_proc cb, void *privptr2)
{
    sl_fd_uniq     = uniq;
    sl_fd_privptr1 = privptr1;
    sl_fd_fd       = fd;
    sl_fd_mask     = mask;
    sl_fd_cb       = cb;
    sl_fd_privptr2 = privptr2;

    return 0;
}
static void WaitForIRQ(const char *argv0, int iohandle, const char *spec)
{
  const char     *p = spec;
  const char     *errp;
  int             all;
  int             expired;

  int             r;
  int             usecs;

  struct timeval  deadline;
  struct timeval  now;
  struct timeval  timeout;
  
  fd_set          xfds;
  
    all = *p == '.';
    p++;
    if (*p == '\0')
        usecs = -1;
    else
    {
        usecs = strtol(p, &errp, 0);
        if (errp == p)
            goto ERR;
    }

    if (usecs >= 0)
    {
        gettimeofday(&deadline, NULL);
        timeval_add_usecs(&deadline, &deadline, usecs);
    }

    was_irq = 0;
    do
    {
        expired = 0;
        if (usecs > 0)
        {
            gettimeofday(&now, NULL);
            expired = (timeval_subtract(&timeout, &deadline, &now) != 0);
        }
        if (usecs <= 0  ||  expired)
            timeout.tv_sec = timeout.tv_usec = 0;

        ////////////
        if (sl_fd_fd >= 0)
        {
            FD_ZERO(&xfds);
            FD_SET(ref, &xfds);
        }
        r = select(sl_fd_fd >= 0? sl_fd_fd + 1 : 0,
                   NULL,
                   NULL,
                   sl_fd_fd >= 0? &xfds        : NULL,
                   usecs >= 0? &timeout : NULL);

        if      (r < 0)
        {
            fprintf(stderr, "%s: select(): %s\n", argv0, strerror(errno));
            exit(2);
        }
        else if (r > 0)
        {
            if (sl_fd_cb != NULL)
                sl_fd_cb(sl_fd_uniq, sl_fd_privptr1,
                         0, sl_fd_fd, SL_EX, sl_fd_privptr2);
        }
        else /* r == 0 */
            expired = 1;

        if (expired  ||  (was_irq  &&  !all)) break;
    } while (1);

    if (!was_irq  &&  !option_quiet)
        printf("# NO IRQ in %d microseconds\n", usecs);

    return;
    
 ERR:
    fprintf(stderr, "%s: error in wait-for-IRQ spec \"%s\"\n", argv0, spec);
    exit(1);
}

static void PerformIO (const char *argv0, int iohandle, const char *spec)
{
  int              r;
  vme_rw_params_t  addr;
  uint32           buf[1000];
  uint8           *p8;
  uint16          *p16;
  uint32          *p32;
  char             format[100];
  uint32           v;
  int              x;
  
    ParseAddrSpec(argv0, "", spec, NULL,
                  &addr, buf, sizeof(buf),
                  PAS_COUNT | PAS_VALUE | PAS_VALARR);
    //PrintAddr(": ", &addr);
    ////for (x=0;x<sizeof(buf)/sizeof(buf[0]);x++)buf[x]=0;
    errno = 0;
    r = do_io(iohandle, &addr, dev.addr_size);
    if (r < 0)
    {
        fprintf(stderr, "%s: IO(\"%s\"): VME error\n", argv0, spec);
        exit(2);
    }

    if (addr.t_op == VME_OP_WRITE)
    {
    }
    else
    {
        if (option_quiet) return;

        if (option_hex_data)
            sprintf(format, "%%s0x%%0%dX", addr.units * 2);
        else
            sprintf(format, "%%s%%d");

        printf("%s=", spec);
        for (x = 0, p8 = p16 = p32 = buf;
             x < addr.count;
             x++)
        {
            if      (addr.units == 1) v = *p8++;
            else if (addr.units == 2) v = *p16++;
            else /* (addr.units == 4)*/v = *p32++;
            printf(format, x > 0? "," : "", v);
        }
        printf("\n");
    }
}

static void exitproc(void)
{
  int  n;

    for (n = 0;  n < onclose_count;  n++)
        do_io(ref, onclose_params + n, dev.addr_size);
}

int main(int argc, char *argv[])
{
  int                      c;              /* Option character */
  int                      err       = 0;  /* ++'ed on errors */

  const char              *p;
  const char              *errp;

    /* Make stdout ALWAYS line-buffered */
    setvbuf(stdout, NULL, _IOLBF, 0);

    while ((c = getopt(argc, argv, "c:hi:qtTx")) > 0)
    {
        switch (c)
        {
            case 'c':
                if (onclose_count != 0)
                {
                    fprintf(stderr, "%s: duplicate on-close specification\n",
                            argv[0]);
                    exit(1);
                }
                p = optarg;
                while (1)
                {
                    ParseAddrSpec(argv[0], "on-close", p, &p,
                                  onclose_params + onclose_count, NULL, 0,
                                  PAS_VALUE | PAS_COMMA_TRM);
                    onclose_params[onclose_count].addr  = &(onclose_params[onclose_count].value);
                    onclose_params[onclose_count].count = 1;
                    onclose_count++;
                    if (*p == '\0') break;
                    if (*p != ',')
                    {
                        fprintf(stderr, "%s: error in on-close spec after [%d]\n",
                                argv[0], onclose_count-1);
                        exit(1);
                    }
                    if (onclose_count < countof(onclose_params))
                        p++; /* Skip ',' */
                    else
                    {
                        fprintf(stderr, "%s: too many on-close actions (limit is %d)\n",
                                argv[0], countof(onclose_params));
                        exit(1);
                    }
                }
                break;
            
            case 'h': goto PRINT_HELP;

            case 'i':
                if (irq_specified)
                {
                    fprintf(stderr, "%s: duplicate IRQ-handling specification\n",
                            argv[0]);
                    exit(1);
                }
                bzero(&irq, sizeof(irq));
                p = optarg;
                irq.n = strtol(p, &errp, 0);
                if (errp == p)
                {
                    fprintf(stderr, "%s: error in IRQ-number spec\n", argv[0]);
                    exit(1);
                }
                p = errp;
                if (*p == '.')
                {
                    p++;
                    irq.vector = strtol(p, &errp, 0);
                    if (errp == p)
                    {
                        fprintf(stderr, "%s: error in IRQ-vector spec\n", argv[0]);
                        exit(1);
                    }
                    p = errp;
                }
                else
                    irq.vector = -1;
                if (*p == ',')
                {
                    p++;
                    if (*p != ','  /* Empty CHECK */  &&
                        *p != '\0' /* or just EOL */)
                    {
                        ParseAddrSpec(argv[0], "IRQ-check", p, &p,
                                      &irq.check, NULL, 0,
                                      PAS_MASK | PAS_VALUE | PAS_VALUE_RQD | PAS_COMMA_TRM);
                    }
                    if (*p == ',')
                    {
                        ParseAddrSpec(argv[0], "IRQ-reset", p + 1, NULL,
                                      &irq.reset, NULL, 0,
                                      PAS_VALUE);
                    }
                    else if (*p != '\0')
                    {
                        fprintf(stderr, "%s: error in IRQ-handling spec \"%s\"\n",
                                argv[0], optarg);
                        exit(1);
                    }
                }
                else if (*p != '\0')
                {
                    fprintf(stderr, "%s: error in IRQ-handling spec \"%s\"\n",
                            argv[0], optarg);
                    exit(1);
                }
                break;

            case 'q': option_quiet    = 1;            break;
            case 't': option_times    = TIMES_REL;    break;
            case 'T': option_times    = TIMES_ISO;    break;

            case 'x': option_hex_data = 1;            break;
            
            case '?':
            default:
                err++;
        }
    }

    if (err) goto ADVICE_HELP;

    if (optind >= argc)
    {
        fprintf(stderr, "%s: device reference is required\n", argv[0]);
        goto ADVICE_HELP;
    }

    ParseDevSpec(argv[0], argv[optind++], &dev);
    
    ref = bivme2_io_open(0, NULL,
                         dev.base_addr, 0, dev.am,
                         irq.n, irq_p);
    if (ref < 0)
    {
        fprintf(stderr, "%s: open(0x%02x:0x%x): r=%d, err=%s\n",
                argv[0], dev.am, dev.base_addr, ref, strerror(errno));
        exit(1);
    }

    atexit(exitproc);

    if (optind >= argc)
        WaitForIRQ(argv[0], ref, ".");
    for (;  optind < argc;  optind++)
    {
        if      (argv[optind][0] == '-')
        {
        }
        else if (argv[optind][0] == ':'  ||  argv[optind][0] == '.')
        {
            WaitForIRQ(argv[0], ref, argv[optind]);
        }
        else
            PerformIO (argv[0], ref, argv[optind]);
    }

    return 0;

 ADVICE_HELP:
    fprintf(stderr, "Try `%s -h' for more information.\n", argv[0]);
    exit(1);

 PRINT_HELP:   
    fprintf(stderr, "Usage: %s [OPTIONS] DEVSPEC [COMMAND...]\n", argv[0]);
    fprintf(stderr, "    OPTIONS:\n");
    fprintf(stderr, "        -c OP1[,OP2]  operations to perform upon close\n");
    fprintf(stderr, "        -h            display this help and exit\n");
    fprintf(stderr, "        -i IRQSPEC    IRQ handling spec (see below)\n");
    fprintf(stderr, "        -q            quiet operation\n");
    fprintf(stderr, "        -t            print relative timestamps\n");
    fprintf(stderr, "        -T            print ISO8601 timestamps\n");
    fprintf(stderr, "        -x            print data in hex\n");
    fprintf(stderr, "    DEVSPEC is ADDRESS_SIZE:ADDRESS_MODIFIER:BASE_OFFSET\n");
    fprintf(stderr, "       (all are decimal; may be in hex, with 0x prefix)\n");
    fprintf(stderr, "    COMMAND format is either U:OFFSET                  # read 1 unit\n");
    fprintf(stderr, "                          or U:OFFSET/COUNT            # read COUNT units\n");
    fprintf(stderr, "                          or U:OFFSET=VALUE{,VALUE...} # write VALUE(s)\n");
    fprintf(stderr, "                          or :[microseconds-to-wait]\n");
    fprintf(stderr, "        - U is either b (bytes), s (shorts, 16bits) or i (ints, 32bits)\n");
    fprintf(stderr, "        - OFFSET is always in bytes; COUNT is in units\n");
    fprintf(stderr, "    IRQ handling: \"-i IRQ_N[.VECTOR][,CHECK[,RESET]]\":\n");
    fprintf(stderr, "        CHECK format is U:OFFSET[@MASK]=VALUE or U:OFFSET+ # !=0\n");
    fprintf(stderr, "        RESET format is U:OFFSET[=VALUE]\n");

    exit(0);
}
