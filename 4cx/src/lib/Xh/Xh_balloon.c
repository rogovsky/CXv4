#include "Xh_includes.h"

#include "XmLiteClue.h"


void XhInitializeBalloon(XhWindow window)
{
    if (window->clue == NULL)
    {
        window->clue = XtVaCreatePopupShell
            ("tipShell", xmLiteClueWidgetClass, window->shell,
             NULL);
    }
}

void XhSetBalloon(CxWidget w, const char *text)
{
  Widget    xw     = CNCRTZE(w);
  XhWindow  window = XhWindowOf(w);
  
    if (window != NULL) XmLiteClueAddWidget(window->clue, xw, text);
}
