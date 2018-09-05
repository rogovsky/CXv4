/*
    Note: the MotifKnobs_tabber_cont.h is a bit bigger than other
          MotifKnobs_*_cont.h files because it must determine availability
          of XmTabStack widget.
 */

#include <Xm/Xm.h>
#if (XmVERSION*1000+XmREVISION) >= 2002
  #define XM_TABBER_AVAILABLE 1
#else
  #define XM_TABBER_AVAILABLE 0
#endif

#if XM_TABBER_AVAILABLE
extern dataknob_cont_vmt_t motifknobs_tabber_vmt;
#endif
