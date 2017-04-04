#include "Xh_includes.h"

#include <Xm/DialogS.h>
#include <Xm/MessageB.h>


static void EliminateIf(Widget box, unsigned char child, int cond)
{
  Widget         w;
  
    if (cond  &&  (w = XmMessageBoxGetChild(box, child)) != NULL)
        XtUnmanageChild(w);
}

static void RemoveDefShadow(Widget box, unsigned char child)
{
  Widget         w;
  
    if ((w = XmMessageBoxGetChild(box, child)) != NULL)
        XtVaSetValues(w,
                      XmNdefaultButtonShadowThickness, 0,
                      NULL);
}

CxWidget XhCreateStdDlg    (XhWindow window, const char *name,
                            const char *title, const char *ok_text,
                            const char *msg, int flags)
{
  Widget         shell;
  Widget         box;
  char           nbuf[100];
  int            mwm_decorations;
  int            mwm_functions;
  unsigned char  dialog_style;
  XmString       s;
  
    /* Validate/sanitize parameters */
    if (name == NULL) name = "-";
    if ((flags & XhStdDlgFResizable) == 0) flags &=~ XhStdDlgFZoomable;

    if (flags & XhStdDlgFNothing) flags &=~
        XhStdDlgFOk | XhStdDlgFCancel | XhStdDlgFHelp;

    /* Create shell */
    mwm_decorations = MWM_DECOR_TITLE | MWM_DECOR_MENU | MWM_DECOR_MINIMIZE;
    mwm_functions   = MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE | MWM_FUNC_CLOSE;
    if (flags & XhStdDlgFResizable)
    {
        mwm_decorations |= MWM_DECOR_BORDER | MWM_DECOR_RESIZEH;
        mwm_functions   |= MWM_FUNC_RESIZE;
    }
    if (flags & XhStdDlgFZoomable)
    {
        mwm_decorations |= MWM_DECOR_MAXIMIZE;
        mwm_functions   |= MWM_FUNC_MAXIMIZE;
    }
    
    snprintf(nbuf, sizeof(nbuf), "%sShell", name);
    shell = XtVaCreatePopupShell(nbuf,
                                 xmDialogShellWidgetClass,
                                 CNCRTZE(XhGetWindowShell(window)),
                                 XmNmwmDecorations,   mwm_decorations,
                                 XmNmwmFunctions,     mwm_functions,
                                 XmNallowShellResize, True,
                                 XmNtitle,            title,
                                 NULL);

    /* Create a box... */
    dialog_style = XmDIALOG_MODELESS;
    if (flags & XhStdDlgFModal) dialog_style = XmDIALOG_PRIMARY_APPLICATION_MODAL;
    
    snprintf(nbuf, sizeof(nbuf), "%sBox", name);
    box = XtVaCreateWidget(nbuf, xmMessageBoxWidgetClass, shell,
                           XmNdialogStyle,       dialog_style,
                           XmNautoUnmanage,      (flags & XhStdDlgFNoAutoUnmng)? False : True,
                           XmNdefaultButtonType, (flags & XhStdDlgFNoDefButton)? XmDIALOG_NONE : XmDIALOG_OK_BUTTON,
                           XmNmessageString,     s = XmStringCreateLtoR(msg != NULL? msg : "", xh_charset),
                           XmNmessageAlignment,  (flags & XhStdDlgFCenterMsg)? XmALIGNMENT_CENTER : XmALIGNMENT_BEGINNING,
                           NULL);
    XmStringFree(s);

    /* ...and tailor it as requested */
    EliminateIf(box, XmDIALOG_MESSAGE_LABEL, msg == NULL);
    EliminateIf(box, XmDIALOG_SEPARATOR,     (flags & XhStdDlgFNothing) != 0);
    EliminateIf(box, XmDIALOG_OK_BUTTON,     (flags & XhStdDlgFOk)      == 0);
    EliminateIf(box, XmDIALOG_CANCEL_BUTTON, (flags & XhStdDlgFCancel)  == 0);
    EliminateIf(box, XmDIALOG_HELP_BUTTON,   (flags & XhStdDlgFHelp)    == 0);
    if (flags & XhStdDlgFNoDefButton)
    {
        RemoveDefShadow(box, XmDIALOG_OK_BUTTON);
        RemoveDefShadow(box, XmDIALOG_CANCEL_BUTTON);
        RemoveDefShadow(box, XmDIALOG_HELP_BUTTON);
    }
    if (ok_text != NULL)
    {
        XtVaSetValues(box, XmNokLabelString, s = XmStringCreateLtoR(ok_text, xh_charset), NULL);
        XmStringFree(s);
    }
    if (flags & XhStdDlgFNoMargins)
        XtVaSetValues(box,
                      XmNshadowThickness, 0,
                      XmNmarginWidth,     0,
                      XmNmarginHeight,    0,
                      NULL);
    
    return ABSTRZE(box);
}

void     XhStdDlgSetMessage(CxWidget dlg, const char *msg)
{
  Widget    dialog = CNCRTZE(dlg);
  XmString  s;
    
    XtVaSetValues(dialog,
                  XmNmessageString, s = XmStringCreateLtoR(msg, xh_charset),
                  NULL);
    XmStringFree(s);
}

void     XhStdDlgShow      (CxWidget dlg)
{
  Widget    dialog = CNCRTZE(dlg);
    
    XtManageChild (dialog);
    XRaiseWindow  (XtDisplay(dialog), XtWindow(XtParent(dialog)));
    
    XSync         (XtDisplay(dialog), False);
    XhENTER_FAILABLE();
    XSetInputFocus(XtDisplay(dialog), XtWindow(XtParent(dialog)), RevertToPointerRoot, CurrentTime);
    XSync         (XtDisplay(dialog), False);
    XhLEAVE_FAILABLE();
}

void     XhStdDlgHide      (CxWidget dlg)
{
  Widget  dialog = CNCRTZE(dlg);
    
    XtUnmanageChild(dialog);
}
