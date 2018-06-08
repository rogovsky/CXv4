#ifndef __CHL_H
#define __CHL_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "Xh.h"

#include "datatree.h"


int  ChlRunSubsystem(DataSubsys subsys, 
                     xh_actdescr_t *toolslist, XhCommandProc commandproc);

int    ChlSaveWindowMode(XhWindow window, const char *path, int options,
                         const char *comment);
int    ChlLoadWindowMode(XhWindow window, const char *path, int options,
                         char *labelbuf, size_t labelbuf_size);

void   ChlRecordEvent   (XhWindow window, const char *format, ...)
                        __attribute__((format (printf, 2, 3)));

/* Error descriptions */
char *ChlLastErr(void);


//// Standard commands

#define CHL_STDCMD_LOAD_MODE "chlLoadMode"
#define CHL_STDCMD_SAVE_MODE "chlSaveMode"
#define CHL_STDCMD_FREEZE    DATATREE_STDCMD_FREEZE
#define CHL_STDCMD_SHOW_HELP "chlShowHelp"
#define CHL_STDCMD_QUIT      "chlQuit"

#define CHL_STDCMD_LOAD_MODE_ACTION "chlLoadModeAction"
#define CHL_STDCMD_SAVE_MODE_ACTION "chlSaveModeAction"

int ChlHandleStdCommand(XhWindow window, const char *cmd, int info_int);


//// Help

enum
{
    CHL_HELP_COLORS = 1 << 0,
    CHL_HELP_KEYS   = 1 << 1,
    CHL_HELP_MOUSE  = 1 << 2,
    CHL_HELP_ALL    = ~0
};
void   ChlShowHelp      (XhWindow window, int parts);
                

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CHL_H */
