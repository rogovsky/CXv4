#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#define XtOpenDisplay __STD_XtOpenDisplay
#include <X11/Intrinsic.h>
#undef XtOpenDisplay
#include <X11/StringDefs.h>

#include "arrow32_src.xbm"
#include "arrow32_msk.xbm"
static XColor  a_fore = {0, 65535,     0,     0};  /* red */
static XColor  a_back = {0, 65535, 65535, 65535};  /* white */


static Widget   (*ptr_XtHooksOfDisplay)     (Display *display);
static Pixmap   (*ptr_XCreateBitmapFromData)(Display *display, Drawable d, char *data, unsigned int width, unsigned int height);
static Cursor   (*ptr_XCreatePixmapCursor)  (Display *display, Pixmap source, Pixmap mask, XColor *foreground_color, XColor *background_color, unsigned int x, unsigned int y);
static int      (*ptr_XFreePixmap)          (Display *display, Pixmap pixmap);
static void     (*ptr_XtAddCallback)        (Widget w, String callback_name, XtCallbackProc callback, XtPointer client_data);
static int      (*ptr_XDefineCursor)        (Display *display, Window w, Cursor cursor);
static Display *(*ptr_XtDisplay)            (Widget w);
static Window   (*ptr_XtWindow)             (Widget w);

static Boolean (*ptr__XtCheckSubclassFlag) (Widget object, _XtXtEnum type_flag);
#define _XtCheckSubclassFlag ptr__XtCheckSubclassFlag

static char *ptr_XtStrings;
#define XtStrings ptr_XtStrings

//#define XtNchangeHook ((char*)&(ptr_XtStrings[2061]))


static void ChangeHookCB(Widget     w __attribute__ ((unused)),
                         XtPointer  closure,
                         XtPointer  call_data)
{
  XtChangeHookDataRec *info = (XtChangeHookDataRec *) call_data;
  Cursor               cur  = (Cursor)                closure;

    if (info->type == XtHrealizeWidget  &&  XtIsShell(info->widget))
    {
        ptr_XDefineCursor(ptr_XtDisplay(info->widget), ptr_XtWindow(info->widget), cur);
    }
}

typedef Display * (*opendisplay_t)(XtAppContext      app_context,
                       String            display_string,
                       String            application_name,
                       String            application_class,
                       XrmOptionDescRec *options,
                       Cardinal          num_options,
                       int              *argc,
                       char             *argv[]);

Display *XtOpenDisplay(XtAppContext      app_context,
                       String            display_string,
                       String            application_name,
                       String            application_class,
                       XrmOptionDescRec *options,
                       Cardinal          num_options,
                       int              *argc,
                       char             *argv[])
{
  Display       *ret;
  opendisplay_t  opendpy;

  Widget         hookobj;
  Pixmap         a_src;
  Pixmap         a_msk;
  Cursor         cur;
  Window         win;

#define GET_SYMBOL(name) \
    do {if ((ptr_##name = dlsym(RTLD_DEFAULT, #name)) == NULL) {fprintf(stderr, "No '"#name"'\n"); return ret;}}while(0)

    opendpy = dlsym(RTLD_NEXT, "XtOpenDisplay");
    if (opendpy == NULL)
    {
        fprintf(stderr, "%s::%s: unable to find original XtOpenDisplay(), dlsym==NULL!\n",
                __FILE__, __FUNCTION__);
        exit(1);
    }
    ret = opendpy(app_context, display_string, 
                  application_name, application_class,
                  options, num_options,
                  argc, argv);
    if (ret == NULL) return ret;

//    fprintf(stderr, "zzz!!!\n");

    GET_SYMBOL(XtStrings);
    GET_SYMBOL(_XtCheckSubclassFlag);

    GET_SYMBOL(XtHooksOfDisplay);
    GET_SYMBOL(XCreateBitmapFromData);
    GET_SYMBOL(XCreatePixmapCursor);
    GET_SYMBOL(XFreePixmap);
    GET_SYMBOL(XtAddCallback);

    GET_SYMBOL(XDefineCursor);
    GET_SYMBOL(XtDisplay);
    GET_SYMBOL(XtWindow);

    hookobj = ptr_XtHooksOfDisplay(ret);
    if (hookobj != NULL)
    {
        win = DefaultRootWindow(ret);
        a_src = ptr_XCreateBitmapFromData(ret, win, arrow32_src_bits, arrow32_src_width, arrow32_src_height);
        a_msk = ptr_XCreateBitmapFromData(ret, win, arrow32_msk_bits, arrow32_msk_width, arrow32_msk_height);
        cur   = ptr_XCreatePixmapCursor  (ret, a_src, a_msk, &a_fore, &a_back, 1, 1);
        ptr_XFreePixmap(ret, a_msk);
        ptr_XFreePixmap(ret, a_src);

        ptr_XtAddCallback(hookobj, XtNchangeHook, ChangeHookCB, (XtPointer)cur);
    }

    return ret;
}
