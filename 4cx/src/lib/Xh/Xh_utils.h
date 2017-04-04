#ifndef __XH_UTILS_H
#define __XH_UTILS_H


#define CNCRTZE(w) ((Widget)w)
#define ABSTRZE(w) ((CxWidget)w)


void attachleft  (Widget what, Widget where, int offset);
void attachright (Widget what, Widget where, int offset);
void attachtop   (Widget what, Widget where, int offset);
void attachbottom(Widget what, Widget where, int offset);


#endif /* __XH_UTILS_H */
