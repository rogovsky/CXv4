#ifndef __CX_STARTER_CON_H
#define __CX_STARTER_CON_H


void ObtainHostIdentity(void);
int IsThisHost         (const char *hostname);
void SetPassiveness    (int option_passive);
void RunCommand        (const char *cmd);


#endif /* __CX_STARTER_CON_H */
