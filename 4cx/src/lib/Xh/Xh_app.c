#include "Xh_includes.h"


#define TOUCH_CURSOR

#ifdef TOUCH_CURSOR
#include "arrow32_src.xbm"
#include "arrow32_msk.xbm"
static XColor  a_fore = {0, 65535,     0,     0};  /* red */
static XColor  a_back = {0, 65535, 65535, 65535};  /* white */

static void ChangeHookCB(Widget     w __attribute__ ((unused)),
                         XtPointer  closure,
                         XtPointer  call_data)
{
  XtChangeHookDataRec *info = (XtChangeHookDataRec *) call_data;
  Cursor               cur  = (Cursor)                closure;

    if (info->type == XtHrealizeWidget  &&  XtIsShell(info->widget))
    {
        XDefineCursor(XtDisplay(info->widget), XtWindow(info->widget), cur);
    }
}
#endif /* TOUCH_CURSOR */

void  XhInitApplication(int         *argc,     char **argv,
                        const char  *app_name,
                        const char  *app_class,
                        char       **fallbacks)
{
    XtSetLanguageProc(NULL, NULL, NULL);
    
    XtToolkitInitialize();
    xh_context = XtCreateApplicationContext();

    XtAppSetFallbackResources(xh_context, fallbacks);

    TheDisplay = XtOpenDisplay(xh_context, NULL,
                               app_name, app_class,
                               NULL, 0,
                               argc, argv);
    if (!TheDisplay) {
        XtWarning ("Can't open display\n");
        exit(0);
    }

    _XhColorPatchXrmDB(TheDisplay);

#ifdef TOUCH_CURSOR
    {
      Widget         hookobj;
      Pixmap         a_src;
      Pixmap         a_msk;
      Cursor         cur;
      Window         win;

        hookobj = XtHooksOfDisplay(TheDisplay);
        if (hookobj != NULL)
        {
            win = DefaultRootWindow(TheDisplay);
            a_src = XCreateBitmapFromData(TheDisplay, win, arrow32_src_bits, arrow32_src_width, arrow32_src_height);
            a_msk = XCreateBitmapFromData(TheDisplay, win, arrow32_msk_bits, arrow32_msk_width, arrow32_msk_height);
            cur   = XCreatePixmapCursor(TheDisplay, a_src, a_msk, &a_fore, &a_back, 1, 1);
            XFreePixmap(TheDisplay, a_msk);
            XFreePixmap(TheDisplay, a_src);

            XtAddCallback(hookobj, XtNchangeHook, ChangeHookCB, (XtPointer)cur);
        }
    }
#endif /* TOUCH_CURSOR */
}

void  XhRunApplication(void)
{
    XtAppMainLoop(xh_context);
}

