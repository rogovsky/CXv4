#ifndef __V2CDR_RENAMES_H
#define __V2CDR_RENAMES_H

#define cdr_unhilite_knob_hook     v2_cdr_unhilite_knob_hook
#define CdrCvtGroupunits2Grouplist v2_CdrCvtGroupunits2Grouplist
#define CdrCvtElemnet2Eleminfo     v2_CdrCvtElemnet2Eleminfo
#define CdrCvtLogchannets2Knobs    v2_CdrCvtLogchannets2Knobs
#define CdrDestroyGrouplist        v2_CdrDestroyGrouplist
#define CdrDestroyEleminfo         v2_CdrDestroyEleminfo
#define CdrDestroyKnobs            v2_CdrDestroyKnobs
#define CdrProcessGrouplist        v2_CdrProcessGrouplist
#define CdrProcessEleminfo         v2_CdrProcessEleminfo
#define CdrProcessKnobs            v2_CdrProcessKnobs
#define CdrActivateKnobHistory     v2_CdrActivateKnobHistory
#define CdrHistorizeKnobsInList    v2_CdrHistorizeKnobsInList
#define CdrSrcOf                   v2_CdrSrcOf
#define CdrFindKnob                v2_CdrFindKnob
#define CdrFindKnobFrom            v2_CdrFindKnobFrom
#define CdrSetKnobValue            v2_CdrSetKnobValue
#define CdrSaveGrouplistMode       v2_CdrSaveGrouplistMode
#define CdrLoadGrouplistMode       v2_CdrLoadGrouplistMode
#define CdrStatGrouplistMode       v2_CdrStatGrouplistMode
#define CdrLogGrouplistMode        v2_CdrLogGrouplistMode
#define CdrStrcolalarmShort        v2_CdrStrcolalarmShort
#define CdrStrcolalarmLong         v2_CdrStrcolalarmLong
#define CdrName2colalarm           v2_CdrName2colalarm
#define CdrLastErr                 v2_CdrLastErr

// descraccess API
#define CdrOpenDescription         v2_CdrOpenDescription
#define CdrCloseDescription        v2_CdrCloseDescription

// NO need in simpleaccess API, as well as in Cdr_script+fqkn

//
#include "src_cx.h"
#include "v2cxlib_renames.h"
#include "v2cda_renames.h"
#include "src_Knobs.h"
#include "v2datatree_renames.h"
#include "src_Cdr.h"


#endif /* __V2CDR_RENAMES_H */
