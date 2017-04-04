#ifndef __CX_STARTER_X11_H
#define __CX_STARTER_X11_H


#include "X11/Xlib.h"


Window  SearchForWindow(Display *dpy, Screen *scr,
                        const char *res_name, const char *title);


#endif /* __CX_STARTER_X11_H */
