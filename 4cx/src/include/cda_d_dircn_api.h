#ifndef __CDA_D_DIRCN_API_H
#define __CDA_D_DIRCN_API_H


#include "cx.h"


typedef int             cda_d_dircn_t;
enum {CDA_D_DIRCN_ERROR = -1};


typedef int (*cda_d_dircn_write_f)(cda_d_dircn_t  var,
                                   void          *privptr,
                                   void          *data,
                                   cxdtype_t      dtype,
                                   int            n_items);

cda_d_dircn_t  cda_d_dircn_register_chan(const char          *name,
                                         cxdtype_t            dtype,
                                         int                  n_items,
                                         void                *addr,
                                         cda_d_dircn_write_f  do_write,
                                         void                *do_write_privptr);

int            cda_d_dircn_update_chan  (cda_d_dircn_t  var,
                                         int            nelems,
                                         rflags_t       rflags);
void           cda_d_dircn_update_cycle (void);

int            cda_d_dircn_chans_count  (void);
int            cda_d_dircn_chan_n_props (cda_d_dircn_t  var,
                                         const char   **name_p,
                                         cxdtype_t     *dtype_p,
                                         int           *n_items_p,
                                         void         **addr_p);
int            cda_d_dircn_override_wr  (cda_d_dircn_t  var,
                                         cda_d_dircn_write_f  do_write,
                                         void                *do_write_privptr);


#endif /* __CDA_D_DIRCN_API_H */
