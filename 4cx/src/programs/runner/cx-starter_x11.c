#include <stdio.h>
#include <string.h>

#include <X11/Xutil.h>

#include "Xh.h"

#include "cx-starter_x11.h"


static Atom  xa_WM_STATE = None;

static Window  SearchOneLevel(Display *dpy, Window win,
                              const char *res_name, const char *title)
{
  Window         ret;
    
  Window        *children;
  unsigned int   nchildren;
  Window         dummy;
  int            i;

  XClassHint     hints;
  char          *winname;

  int            r;
  Atom           type;
  int            format;
  unsigned long  nitems, after;
  unsigned char *data;
  

    if (!XQueryTree(dpy, win, &dummy, &dummy, &children, &nchildren))
        return 0;

    for (i = 0, ret = 0; i < nchildren  &&  ret == 0;  i++)
    {
        /* Does this window have a WM_STATE property? */
        XhENTER_FAILABLE();

        type = None;
        r = XGetWindowProperty(dpy, children[i], xa_WM_STATE,
                               0, 0, False,
                               AnyPropertyType, &type, &format, &nitems,
                               &after, &data);

        if (r != Success)
        {
        }
        else if (type)
        {
            /* Yes, it has WM_STATE! */

            XFree(data); // We no longer need obtained data

            if      (res_name != NULL  &&  res_name[0] != '\0')
            {
                /* Check window name... */
                if (XGetClassHint(dpy, children[i], &hints))
                {
                    if (hints.res_name != NULL  &&
                        strcmp(res_name, hints.res_name) == 0) ret = children[i];

                    if (hints.res_name  != NULL) XFree(hints.res_name);
                    if (hints.res_class != NULL) XFree(hints.res_class);
                }
            }
            else if (title != NULL  &&  title[0] != '\0')
            {
                if (XFetchName(dpy, children[i], &winname))
                {
                    if (strcmp(title, winname) == 0) ret = children[i];
                    XFree(winname);
                }
            }
        }
        else
        {
            /* No, we should recurse deeper */
            ret = SearchOneLevel(dpy, children[i], res_name, title);
        }
        
        XhLEAVE_FAILABLE();
    }
    
    if (children) XFree(children);
    
    return ret;
}

Window  SearchForWindow(Display *dpy, Screen *scr,
                        const char *res_name, const char *title)
{
  Window        root    = RootWindowOfScreen(scr);

    if (xa_WM_STATE == None)
    {
        xa_WM_STATE = XInternAtom(dpy, "WM_STATE", True);
        if (xa_WM_STATE == None) return 0;
    }

    return SearchOneLevel(dpy, root, res_name, title);
}

