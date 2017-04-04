
#include "Xh_includes.h"

#include <Xm/FileSB.h>
#include <Xm/List.h>
#include <Xm/MessageB.h>
#include <Xm/SelectioB.h>

/* For LoadDlg only... */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>


static void DoOkCancel(Widget     w,
                       XtPointer  closure,
                       int        info_int)
{
  XhWindow       window = XhWindowOf(ABSTRZE(w));
  const char    *cmd    = closure;
  XhCommandProc  proc;
  
    if (window != NULL)
    {
        proc = window->commandproc;
        if (proc != NULL)
            proc(window, cmd, info_int);
    }
}

static void OkCB      (Widget     w,
                       XtPointer  closure,
                       XtPointer  call_data __attribute__((unused)))
{
    DoOkCancel(w, closure, 0);
}

static void CancelCB  (Widget     w,
                       XtPointer  closure,
                       XtPointer  call_data __attribute__((unused)))
{
    DoOkCancel(w, closure, -1);
}

static void EliminateChild(Widget dialog, unsigned char child)
{
  Widget w = XmSelectionBoxGetChild(dialog, child);

    if (w != NULL) XtUnmanageChild(w);
}

CxWidget XhCreateFdlg(XhWindow window, const char *name,
                      const char *title, const char *ok_text,
                      const char *pattern, const char *cmd)
{
  Widget    dialog;
  Arg       al[10];
  int       ac;
  XmString  title_s   = NULL;
  XmString  pattern_s = NULL;
  XmString  ok_s      = NULL;

    ac = 0;
    XtSetArg(al[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL);       ac++;
    if (title   != NULL)
        {XtSetArg(al[ac], XmNdialogTitle,   title_s   = XmStringCreate    (title,   xh_charset)); ac++;}
    if (pattern != NULL)
        {XtSetArg(al[ac], XmNpattern,       pattern_s = XmStringCreate    (pattern, xh_charset)); ac++;}
    if (ok_text != NULL)
        {XtSetArg(al[ac], XmNokLabelString, ok_s      = XmStringCreateLtoR(ok_text, xh_charset)); ac++;}
        

    dialog = XmCreateFileSelectionDialog(window->shell, name, al, ac);
    EliminateChild(dialog, XmDIALOG_HELP_BUTTON);

    if (pattern_s != NULL) XmStringFree(pattern_s);
    if (title_s   != NULL) XmStringFree(title_s);
    if (ok_s      != NULL) XmStringFree(ok_s);
    
    XtAddCallback(dialog, XmNokCallback,     OkCB,     cmd);
    XtAddCallback(dialog, XmNcancelCallback, CancelCB, cmd);

    return ABSTRZE(dialog);
}


void     XhFdlgShow       (CxWidget  fdlg)
{
  Widget    dialog = CNCRTZE(fdlg);
    
    if (XtIsManaged(dialog)) return;
    
    XmFileSelectionDoSearch(dialog, NULL);
    XtManageChild(dialog);
}


void     XhFdlgHide       (CxWidget  fdlg)
{
  Widget  dialog = CNCRTZE(fdlg);
    
    XtUnmanageChild(dialog);
}


void     XhFdlgGetFilename(CxWidget  fdlg, char *buf, size_t bufsize)
{
  Widget    dialog = CNCRTZE(fdlg);
  XmString  fileString;
  XmString  dirString;
  char     *file_s;
  char     *dir_s;
  
    XtVaGetValues(dialog,
                  XmNdirSpec,   &fileString,
                  XmNdirectory, &dirString,
                  NULL);
    
    XmStringGetLtoR(fileString, xh_charset, &file_s);
    if (file_s[0] == '/')
    {
        strzcpy(buf, file_s, bufsize);
    }
    else
    {
        XmStringGetLtoR(dirString, xh_charset, &dir_s);
        snprintf(buf, bufsize, "%s%s", dir_s, file_s);
        XtFree(dir_s);
    }
    XtFree(file_s);
    XmStringFree(fileString);
    XmStringFree(dirString);
}


CxWidget XhCreateSaveDlg(XhWindow window, const char *name,
                              const char *title, const char *ok_text,
                              const char *cmd)
{
  Widget    dialog;
  Arg       al[20];
  int       ac;
  XmString  msg_s     = NULL;
  XmString  title_s   = NULL;
  XmString  ok_s      = NULL;

    ac = 0;
    XtSetArg(al[ac], XmNdialogStyle,   XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
    XtSetArg(al[ac], XmNautoUnmanage,  False);                              ac++;
    XtSetArg(al[ac], XmNtextColumns,   40);                                 ac++;
    XtSetArg(al[ac], XmNselectionLabelString, msg_s = XmStringCreateLtoR("Comment", xh_charset)); ac++;
    if (title   != NULL)
        {XtSetArg(al[ac], XmNdialogTitle,   title_s   = XmStringCreate    (title,   xh_charset)); ac++;}
    if (ok_text != NULL)
        {XtSetArg(al[ac], XmNokLabelString, ok_s      = XmStringCreateLtoR(ok_text, xh_charset)); ac++;}

    dialog = XmCreatePromptDialog(window->shell, name, al, ac);

    EliminateChild(dialog, XmDIALOG_HELP_BUTTON);

    XtAddCallback(dialog, XmNokCallback,     OkCB,     cmd);
    XtAddCallback(dialog, XmNcancelCallback, CancelCB, cmd);

    return ABSTRZE(dialog);
}

void     XhSaveDlgShow       (CxWidget dlg)
{
  Widget    dialog = CNCRTZE(dlg);
  XmString  s;

    if (XtIsManaged(dialog)) return;

    XtVaSetValues(dialog, XmNtextString, s = XmStringCreateLtoR("", xh_charset), NULL);
    XmStringFree(s);
    XtManageChild(dialog);
}

void     XhSaveDlgHide       (CxWidget dlg)
{
  Widget  dialog = CNCRTZE(dlg);
    
    XtUnmanageChild(dialog);
}

void     XhSaveDlgGetComment (CxWidget dlg, char *buf, size_t bufsize)
{
  Widget  dialog = CNCRTZE(dlg);
  char   *vs;

    XtVaGetValues(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT),
                  XmNvalue, &vs,
                  NULL);

    strzcpy(buf, vs, bufsize);

    XtFree(vs);
}

/*
 *  Note:
 *      "pattern" and "buf" MAY point to the same space -- it is safe.
 */

void     XhFindFilename      (const char *pattern,
                              char *buf, size_t bufsize)
{
  const char         *p;
  const char         *prefix;
  size_t              prefixlen;
  const char         *suffix;
  size_t              suffixlen;
  time_t              timenow = time(NULL);
  struct tm          *tp;
  char                filename[PATH_MAX];
  int                 sofs;

  enum {NDIGITS = 3};
  char                sstr[NDIGITS+1];
  int                 maxserial;
  int                 x;
  struct stat         statbuf;

    p = strchr(pattern, '*');
    if (p == NULL)
    {
        *buf = '\0';
        return;
    }
    
    prefix    = pattern;
    prefixlen = p - pattern;
    suffix    = p + 1;
    suffixlen = strlen(pattern) - prefixlen - 1;

    tp = localtime(&timenow);
    
    snprintf(filename, sizeof(filename), "%*s%04d%02d%02d_%*s%s",
             (int)prefixlen, "",
             tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday,
             NDIGITS, "",
             suffix);
    memcpy(filename, prefix, prefixlen);
    sofs = prefixlen + strlen("20041130_");

    for (maxserial = 1, x = 1;  x <= NDIGITS;  x++) maxserial *= 10;
    --maxserial;

    for (x = 0;  x <= maxserial;  x++)
    {
        sprintf(sstr, "%0*d", NDIGITS, x);
        memcpy(filename + sofs, sstr, NDIGITS);

        /* Note: "lstat" instead of "stat" because we want to find
         an unused FILENAME, not a non-existing FILE. */
        if (lstat(filename, &statbuf) != 0) break;
    }
    if (x > maxserial) memset(filename + sofs, 'Z', NDIGITS);
    
    strzcpy(buf, filename, bufsize);
}


typedef struct
{
    time_t      cr_time;
    const char *filename;
    const char *comment;
} loaddlg_fileinfo_t;

typedef struct
{
    Widget                  filedpy;
    xh_loaddlg_filter_proc  filter;
    void                   *privptr;
    const char             *pattern;
    
    loaddlg_fileinfo_t     *list;
    int                     list_allocd;
    int                     list_used;
} loaddlg_rec_t;

static void LoadDlgSelectionCB(Widget     w  __attribute__((unused)),
                               XtPointer  closure,
                               XtPointer  call_data)
{
  Widget                dialog  = (Widget) closure;
  XmListCallbackStruct *info    = (XmListCallbackStruct *) call_data;
  int                   n       = info == NULL? 0 : info->item_position - 1;
  loaddlg_rec_t        *privrec;
  const char           *filename;
  
    XtVaGetValues(dialog, XmNuserData, &privrec, NULL);
    filename = privrec->list[n].filename;
    if (filename == NULL) filename = "???";
    
    XmTextSetString(privrec->filedpy, filename);
}

CxWidget XhCreateLoadDlg     (XhWindow window, const char *name,
                              const char *title, const char *ok_text,
                              xh_loaddlg_filter_proc filter, void *privptr,
                              const char *pattern,
                              const char *cmd, const char *flst_cmd)
{
  Widget         dialog;
  Arg            al[20];
  int            ac;
  XmString       msg_s     = NULL;
  XmString       title_s   = NULL;
  XmString       ok_s      = NULL;
  Widget         w;

  loaddlg_rec_t *privrec;

    privrec = (void *)(XtMalloc(sizeof(*privrec)));
    if (privrec == NULL) return NULL;
    bzero(privrec, sizeof(*privrec));

    privrec->filter  = filter;
    privrec->privptr = privptr;
    privrec->pattern = XtNewString(pattern);
    
    ac = 0;
    XtSetArg(al[ac], XmNuserData,      (char *)privrec);                    ac++;
    XtSetArg(al[ac], XmNdialogStyle,   XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
    XtSetArg(al[ac], XmNautoUnmanage,  False);                              ac++;
    XtSetArg(al[ac], XmNlistLabelString, msg_s = XmStringCreateLtoR("Files", xh_charset)); ac++;
    if (title   != NULL)
        {XtSetArg(al[ac], XmNdialogTitle,   title_s   = XmStringCreate    (title,   xh_charset)); ac++;}
    if (ok_text != NULL)
        {XtSetArg(al[ac], XmNokLabelString, ok_s      = XmStringCreateLtoR(ok_text, xh_charset)); ac++;}

    dialog = XmCreateSelectionDialog(window->shell, name, al, ac);

    EliminateChild(dialog, XmDIALOG_HELP_BUTTON);
    EliminateChild(dialog, XmDIALOG_APPLY_BUTTON);
    EliminateChild(dialog, XmDIALOG_TEXT);
    EliminateChild(dialog, XmDIALOG_SELECTION_LABEL);
    
    privrec->filedpy =
        XtVaCreateManagedWidget("text_o", xmTextWidgetClass,
                                dialog,
                                XmNscrollHorizontal,      False,
                                XmNcursorPositionVisible, False,
                                XmNeditable,              False,
                                XmNtraversalOn,           False,
                                XmNvalue,                 " ",
                                NULL);

    if (flst_cmd != NULL)
    {
        w = XtVaCreateManagedWidget("showFiles", xmPushButtonWidgetClass,
                                    dialog,
                                    NULL);
        XtAddCallback(w, XmNactivateCallback, OkCB, flst_cmd);
    }

    w = XmSelectionBoxGetChild(dialog, XmDIALOG_LIST);
    if (w != NULL)
        XtAddCallback(w, XmNbrowseSelectionCallback,
                      LoadDlgSelectionCB, (XtPointer)dialog);
    
    XtAddCallback(dialog, XmNokCallback,     OkCB,     cmd);
    XtAddCallback(dialog, XmNcancelCallback, CancelCB, cmd);

    return ABSTRZE(dialog);
}

static int loaddlg_fileinfo_compare(const void *a, const void *b)
{
  const loaddlg_fileinfo_t *ra = (const loaddlg_fileinfo_t *)a;
  const loaddlg_fileinfo_t *rb = (const loaddlg_fileinfo_t *)b;
  
    /* Idea of one-line 'if'-less comparison from libc.info('Comparison Functions') */
    return -((ra->cr_time > rb->cr_time) - (ra->cr_time < rb->cr_time));
}

void     XhLoadDlgShow       (CxWidget dlg)
{
  Widget              dialog = CNCRTZE(dlg);
  loaddlg_rec_t      *privrec;

  DIR                *dp;
  struct dirent      *entry;
  struct stat         statbuf;
  const char         *p;
  const char         *prefix;
  size_t              prefixlen;
  const char         *suffix;
  size_t              suffixlen;

  time_t              cr_time;
  char                comment[200];

  loaddlg_fileinfo_t *lp;
  int                 idx;

  XmString           *strlist;
  char                buf[300];
  
  enum {BUF_ALLOCD_INC = 100};
  
    if (XtIsManaged(dialog)) return;
  
    XtVaGetValues(dialog, XmNuserData, &privrec, NULL);

    /* Destroy previous list, if any */
    for (lp = privrec->list, idx = 0;
                             idx < privrec->list_used;
         lp++,               idx++)
    {
        if (lp->filename != NULL) XtFree(lp->filename);
        if (lp->comment  != NULL) XtFree(lp->comment);
        bzero(lp, sizeof(*lp));
    }
    privrec->list_used = 0;

    /* Scan the dir */
    dp = opendir(".");
    if (dp == NULL)
    {
        fprintf(stderr, "%s(): opendir(): %s\n", __FUNCTION__, strerror(errno));
    }
    else
    {
        p = strchr(privrec->pattern, '*');
        if (p == NULL)
        {
            prefix    = suffix    = NULL;
            prefixlen = suffixlen = 0;
        }
        else
        {
            prefix    = privrec->pattern;
            prefixlen = p - privrec->pattern;
            suffix    = p + 1;
            suffixlen = strlen(privrec->pattern) - prefixlen - 1;
        }
        
        while ((entry = readdir(dp)) != NULL)
        {
            if (strlen(entry->d_name) < prefixlen + suffixlen
                ||
                (prefixlen != 0  &&
                memcmp(entry->d_name, prefix, prefixlen) != 0)
                ||
                (suffixlen != 0  &&
                memcmp(entry->d_name + strlen(entry->d_name) - suffixlen, suffix, suffixlen) != 0)
               ) continue;
            
            if (stat(entry->d_name, &statbuf) < 0  ||
                !S_ISREG(statbuf.st_mode)) continue;

            cr_time    = statbuf.st_mtime;
            comment[0] = '\0';
            
            if (privrec->filter(entry->d_name,
                                &cr_time, comment, sizeof(comment),
                                privrec->privptr) != 0) continue;
            
            /* Okay, if there isn't enough room, allocate more */
            if (privrec->list_used == privrec->list_allocd)
            {
                lp = XtRealloc(privrec->list,
                               sizeof(*lp) * (privrec->list_allocd + BUF_ALLOCD_INC));
                if (lp == NULL) break;
                privrec->list = lp;
                privrec->list_allocd += BUF_ALLOCD_INC;
            }

            /* Fill in the data... */
            lp = privrec->list + privrec->list_used;
            bzero(lp, sizeof(*lp));
            lp->cr_time  = cr_time;
            lp->filename = XtNewString(entry->d_name);
            lp->comment  = comment[0] == '\0'? NULL : XtNewString(comment);
            privrec->list_used++;
        }
        closedir(dp);
    }

    qsort(privrec->list, privrec->list_used,
          sizeof(loaddlg_fileinfo_t),
          loaddlg_fileinfo_compare);

    strlist = XtCalloc(privrec->list_used, sizeof(XmString));

    if (strlist != NULL)
    {
        for (lp = privrec->list, idx = 0;
             idx < privrec->list_used;
             lp++,               idx++)
        {
            snprintf(buf, sizeof(buf), "%s:  %s",
                     stroftime(lp->cr_time, " "),
                     lp->comment != NULL? lp->comment : lp->filename);
            
            strlist[idx] = XmStringCreate(buf, xh_charset);
        }

        XtVaSetValues(dialog,
                      XmNlistItemCount, privrec->list_used,
                      XmNlistItems,     strlist,
                      NULL);

        if (privrec->list_used > 0)
            XmListSelectPos(XmSelectionBoxGetChild(dialog, XmDIALOG_LIST),
                            1, True);
        
        for (idx = 0;  idx < privrec->list_used;  idx++)
            XmStringFree(strlist[idx]);
        XtFree(strlist);
    }

    XtManageChild(dialog);
}

void     XhLoadDlgHide       (CxWidget dlg)
{
  Widget  dialog = CNCRTZE(dlg);

    XtUnmanageChild(dialog);
}

void     XhLoadDlgGetFilename(CxWidget dlg, char *buf, size_t bufsize)
{
  Widget              dialog = CNCRTZE(dlg);
  loaddlg_rec_t      *privrec;
  int                 count;
  unsigned int       *poslist;

    XtVaGetValues(dialog, XmNuserData, &privrec, NULL);
    XtVaGetValues(XmSelectionBoxGetChild(dialog, XmDIALOG_LIST),
                  XmNselectedPositionCount, &count,
                  XmNselectedPositions,     &poslist,
                  NULL);
    
    if (count < 1)
        *buf = '\0';
    else
        strzcpy(buf, privrec->list[poslist[0] - 1].filename, bufsize);
}

