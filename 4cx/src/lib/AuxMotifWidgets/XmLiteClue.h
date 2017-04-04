/* 
LiteClue.h - Public definitions for LiteClue widget
	See LiteClue documentation

Copyright 1996 COMPUTER GENERATION, INC.,

The software is provided "as is", without warranty of any kind, express
or implied, including but not limited to the warranties of
merchantability, fitness for a particular purpose and noninfringement.
In no event shall Computer Generation, inc. nor the author be liable for
any claim, damages or other liability, whether in an action of contract,
tort or otherwise, arising from, out of or in connection with the
software or the use or other dealings in the software.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation.

Author:
Gary Aviv 
Computer Generation, Inc.,
gary@compgen.com
*/
/* Revision History:
$Log: LiteClue.h,v $
Revision 1.5  1998/09/07 14:06:24  gary
Added const to prototype of XcgLiteClueAddWidget at request from user

Revision 1.4  1997/06/15 14:07:56  gary
Added XcgLiteClueDispatchEvent

Revision 1.3  1997/04/14 13:03:25  gary
Added XgcNwaitperiod XgcNcancelWaitPeriod and c++ wrappers

Revision 1.2  1996/10/20 13:39:25  gary
Version 1.2 freeze

Revision 1.1  1996/10/19 16:08:04  gary
Initial


$log
Added const to prototype of XcgLiteClueAddWidget at request from user
$log
*/

#ifndef _XMLITECLUE_H
#define _XMLITECLUE_H
#include <X11/StringDefs.h>

/*
 * New resource names
 */

#define XmNcancelWaitPeriod	 "cancelWaitPeriod"
#define XmNwaitPeriod	 "waitPeriod"
/*
 * New resource classes
 */
#define XmCCancelWaitPeriod	"cancelWaitPeriod"
#define XmCWaitPeriod	"WaitPeriod"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" { 
#endif

extern WidgetClass xmLiteClueWidgetClass; 
typedef struct _LiteClueClassRec *XmLiteClueWidgetClass;
typedef struct _LiteClueRec      *XmLiteClueWidget;
void XmLiteClueAddWidget(Widget w, Widget watch, const char * text);
void XmLiteClueDeleteWidget(Widget w, Widget watch);
void XmLiteClueSetSensitive(Widget w, Widget watch, Boolean sensitive);
Boolean XmLiteClueGetSensitive(Widget w, Widget watch);
Boolean XmLiteClueDispatchEvent(Widget w, XEvent  *event);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _XMLITECLUE_H */
