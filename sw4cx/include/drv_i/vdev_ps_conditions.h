#ifndef __VDEV_PS_CONDITIONS
#define __VDEV_PS_CONDITIONS


/*********************************************************************

  This file defines values for "vdev_condition" channels of
  vdev-based drivers for power supplies.

*********************************************************************/

enum
{
    VDEV_PS_CONDITION_OFFLINE   = -99,
    VDEV_PS_CONDITION_INTERLOCK = -2,
    VDEV_PS_CONDITION_CUT_OFF   = -1,
    VDEV_PS_CONDITION_IS_OFF    =  0,
    VDEV_PS_CONDITION_IS_ON     = +1,
};


#endif /* __VDEV_PS_CONDITIONS */
