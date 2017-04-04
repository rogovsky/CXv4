#ifndef __V2CDA_RENAMES_H
#define __V2CDA_RENAMES_H


#define cda_TMP_register_physinfo_dbase v2_cda_TMP_register_physinfo_dbase
#define cda_new_server                  v2_cda_new_server
#define cda_add_auxsid                  v2_cda_add_auxsid
#define cda_run_server                  v2_cda_run_server
#define cda_hlt_server                  v2_cda_hlt_server
#define cda_del_server                  v2_cda_del_server
#define cda_continue                    v2_cda_continue
#define cda_add_evproc                  v2_cda_add_evproc
#define cda_del_evproc                  v2_cda_del_evproc
#define cda_get_reqtimestamp            v2_cda_get_reqtimestamp
#define cda_set_physinfo                v2_cda_set_physinfo
#define cda_cyclesize                   v2_cda_cyclesize
#define cda_status_lastn                v2_cda_status_lastn
#define cda_status_of                   v2_cda_status_of
#define cda_status_srvname              v2_cda_status_srvname
#define cda_srcof_physchan              v2_cda_srcof_physchan
#define cda_srcof_formula               v2_cda_srcof_formula
#define cda_sidof_physchan              v2_cda_sidof_physchan
#define cda_cnsof_physchan              v2_cda_cnsof_physchan
#define cda_cnsof_formula               v2_cda_cnsof_formula
#define cda_add_physchan                v2_cda_add_physchan
#define cda_register_formula            v2_cda_register_formula
#define cda_HACK_findnext_f             v2_cda_HACK_findnext_f
#define cda_setphyschanval              v2_cda_setphyschanval
#define cda_setphyschanraw              v2_cda_setphyschanraw
#define cda_prgsetphyschanval           v2_cda_prgsetphyschanval
#define cda_prgsetphyschanraw           v2_cda_prgsetphyschanraw
#define cda_getphyschanval              v2_cda_getphyschanval
#define cda_getphyschanraw              v2_cda_getphyschanraw
#define cda_getphyschanvnr              v2_cda_getphyschanvnr
#define cda_getphyschan_rd              v2_cda_getphyschan_rd
#define cda_getphyschan_q               v2_cda_getphyschan_q
#define cda_execformula                 v2_cda_execformula
#define cda_setlclreg                   v2_cda_setlclreg
#define cda_getlclreg                   v2_cda_getlclreg
#define cda_set_lclregchg_cb            v2_cda_set_lclregchg_cb
#define cda_set_strlogger               v2_cda_set_strlogger
#define cda_lasterr                     v2_cda_lasterr

//
#include "src_cx.h"
#include "src_cda.h"


// stripped_cda-specific
int stripped_cda_register_physinfo_dbase(cda_serverid_t sid,
                                         physinfodb_rec_t *db);
int  stripped_cda_getphyschan_q_int(cda_physchanhandle_t  chanh, int *qp);


#endif /* __V2CDA_RENAMES_H */
