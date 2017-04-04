#include "Xh_includes.h"

#define USE_XMTEXT 1

void XhCreateStatusline(XhWindow window)
{
  Dimension  fst;
  Widget     form, line, sepr;

    XtVaGetValues(window->mainForm, XmNshadowThickness, &fst, NULL);
  
    form = XtVaCreateManagedWidget("statusLineForm", xmFormWidgetClass, window->mainForm,
                                   XmNleftAttachment,   XmATTACH_FORM,
                                   XmNrightAttachment,  XmATTACH_FORM,
                                   XmNbottomAttachment, XmATTACH_FORM,
                                   XmNrightOffset,      fst,
                                   XmNleftOffset,       fst,
                                   XmNbottomOffset,     fst,
                                   NULL);
#if USE_XMTEXT
    line = XtVaCreateManagedWidget("statusLine", xmTextWidgetClass, form,
                                   XmNvalue,                 "",
                                   XmNtraversalOn,           False,
                                   XmNeditable,              False,
                                   XmNcursorPositionVisible, False,
                                   XmNshadowThickness,       0,
                                   XmNbottomAttachment,      XmATTACH_FORM,
                                   XmNleftAttachment,        XmATTACH_FORM,
                                   XmNrightAttachment,       XmATTACH_FORM,
                                   XmNrightOffset,           1,
                                   XmNleftOffset,            1,
                                   XmNbottomOffset,          1,
                                   NULL);
#else
    line = XtVaCreateManagedWidget("statusLine", xmLabelWidgetClass, form,
                                   XmNalignment,        XmALIGNMENT_BEGINNING,
                                   XmNlabelString,      XmStringCreate(" ", xh_charset),
                                   XmNrecomputeSize,    False,
                                   XmNbottomAttachment, XmATTACH_FORM,
                                   XmNleftAttachment,   XmATTACH_FORM,
                                   XmNrightAttachment,  XmATTACH_FORM,
                                   XmNrightOffset,      1,
                                   XmNleftOffset,       1,
                                   XmNbottomOffset,     1,
                                   NULL);
#endif

    sepr = XtVaCreateManagedWidget("", xmSeparatorWidgetClass, form,
                                   XmNorientation,      XmHORIZONTAL,
                                   XmNleftAttachment,   XmATTACH_FORM,
                                   XmNrightAttachment,  XmATTACH_FORM,
                                   XmNbottomAttachment, XmATTACH_WIDGET,
                                   XmNleftOffset,       0,
                                   XmNrightOffset,      0,
                                   XmNbottomOffset,     0,
                                   XmNbottomWidget,     line,
                                   NULL);

    window->statsForm = form;
    window->statsLine = line;
}

void XhMakeMessage    (XhWindow window, const char *format, ...)
{
  va_list   ap;

    va_start(ap, format);
    XhVMakeMessage(window, format, ap);
    va_end(ap);
}

void XhVMakeMessage   (XhWindow window, const char *format, va_list ap)
{
  char      buf[4096];

    if (format != NULL)
    {
        vsnprintf(buf, sizeof(buf), format, ap);

        strzcpy(window->mainmessage, buf, sizeof(window->mainmessage));
    }
    else
        window->mainmessage[0] = '\0';

    if (window->statsLine != NULL)
    {
#if USE_XMTEXT
        XmTextSetString(window->statsLine, window->mainmessage);
#else
     XmString  s;
     
        XtVaSetValues(window->statsLine,
                      XmNlabelString, s = XmStringCreate(window->mainmessage, xh_charset),
                      NULL);
        XmStringFree(s);
#endif
    }
    else
    {
        fprintf(stderr, "WARNING: %s() called with absent statusline!\nThe message is \"%s\"\n",
                __FUNCTION__, window->mainmessage);
    }
    
}

void XhMakeTempMessage(XhWindow window, const char *format, ...)
{
  va_list   ap;

    va_start(ap, format);
    XhVMakeTempMessage(window, format, ap);
    va_end(ap);
}

void XhVMakeTempMessage(XhWindow window, const char *format, va_list ap)
{
  char      buf[4096];

    /* First, format a message */
    if (format != NULL)
    {
        vsnprintf(buf, sizeof(buf), format, ap);

        strzcpy(window->tempmessage, buf, sizeof(window->tempmessage));
    }
    else
        window->tempmessage[0] = '\0';

    if (window->statsLine == NULL) return;
    
    /* Now, if it is non-null, make it visible... */
    if (window->tempmessage[0] != '\0')
    {
#if USE_XMTEXT
        XmTextSetString(window->statsLine, window->tempmessage);
#else
      XmString  s;
    
        s = XmStringCreate(window->tempmessage, xh_charset);
        XtVaSetValues(window->statsLine, XmNlabelString, s, NULL);
        XmStringFree(s);
#endif
    }
    /* Otherwise restore the main message */
    else
    {
        /* Note: passing `makemessage' is valid, since sprintf()
         is made into XhMakeMessage's internal buffer, but not to
         `window->mainmessage' directly. */
        XhMakeMessage(window, "%s", window->mainmessage);
    }
}
