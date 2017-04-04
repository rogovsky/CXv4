#include "Chl_includes.h"

#include <Xm/MwmUtil.h>

#include <Xm/DialogS.h>
#include <Xm/Frame.h>
#include <Xm/MessageB.h>


typedef help_rec_t dlgrec_t;


typedef struct
{
    int         fgcol, bgcol;
    const char *label;
    const char *comment;
} colhelpline_t;

static colhelpline_t colhelp_contents[] =
{
    {-1, XH_COLOR_BG_NORM_YELLOW, "Желтый",        "Значение вне нормального диапазона"},
    {-1, XH_COLOR_BG_NORM_RED,    "Красный",       "Значение вне безопасного диапазона"},
    {XH_COLOR_FG_WEIRD,
         XH_COLOR_BG_WEIRD,       "Странный",      "Значение странно"},
    {-1, XH_COLOR_BG_DEFUNCT,     "Гусиной кожи",  "Значение давно не обновляется"},
    {-1, XH_COLOR_BG_HWERR,       "Бордовый",      "Аппаратная проблема (щелчок правой кнопкой мыши на значении - детали)"},
    {-1, XH_COLOR_BG_SFERR,       "Болотный",      "Программная проблема (щелчок правой кнопкой мыши на значении - детали)"},
    {-1, XH_COLOR_BG_OTHEROP,     "Оранжевый",     "Значение меняется другим оператором"},
    {-1, XH_COLOR_BG_JUSTCREATED, "Бледноголубой", "Данные еще ни разу не были прочитаны"},
    {-1, XH_COLOR_JUST_GREEN,     "Зеленый",       "Только что погасла лампочка сигнализации"},
    {-1, XH_COLOR_BG_PRGLYCHG,    "Бледнозеленый", "Значение менялось программой"},
    {-1, XH_COLOR_BG_NOTFOUND,    "Темносерый",    "Канал не найден"},
};

typedef struct
{
    const char *keys;
    const char *comment;
} keyhelpline_t;

static keyhelpline_t keyhelp_contents[] =
{
    {"<Tab>/Shift+<Tab>",
        "Перемещение по каналам"},
    {"Стрелки вверх/вниз\n(в текстовом поле)",
        "Увеличение/уменьшение значения на шаг\n"
            "Удержание при этом Ctrl уменьшает шаг в 10 раз\n"
            "Удержание Alt увеличивает шаг в 10 раз"},
    {"<Enter>",
        "Завершение ввода"},
    {"<Esc>",
        "Отмена ввода (ДО нажатия <Enter>!)"},
    {"", NULL},
    {"<Правая кнопка мыши>\nAlt+<Enter>\nShift+<F10>",
        "Вызов окна \xabСвойства канала\xbb"},
    {"Ctrl+<Правая кнопка мыши>\nCtrl+<Пробел>",
        "Вывод значения крупным шрифтом"},
    {"Shift+<Правая кнопка мыши>\nShift+<Пробел>",
        "Отправка канала на график-самописец"},
};

void   ChlShowHelp      (XhWindow window, int parts)
{
  chl_privrec_t *privrec = _ChlPrivOf(window);
  dlgrec_t      *rec = &(privrec->hr);

  Widget         w;
  Widget         form;
  Widget         frame;
  Widget         grid;
  Widget         prev = NULL;
  XmString       s;
  colhelpline_t *clp;
  keyhelpline_t *klp;
  int            row;
  int            fg, bg;
  
    if (!(rec->initialized))
    {
        rec->box = XhCreateStdDlg(window, "help", "Help", NULL, "Краткая справка",
                                  XhStdDlgFOk);

        form = XtVaCreateManagedWidget("form", xmFormWidgetClass, rec->box,
                                       XmNshadowThickness, 0,
                                       NULL);

        /* List of colors */
        if (parts & CHL_HELP_COLORS)
        {
            frame = XtVaCreateManagedWidget("frame", xmFrameWidgetClass, form,
                                            XmNmarginWidth, 1,
                                            NULL);
            attachtop(frame, prev, 0);
            prev = frame;
            
            XtVaCreateManagedWidget("title", xmLabelWidgetClass, frame,
                                    XmNframeChildType, XmFRAME_TITLE_CHILD,
                                    XmNlabelString, s = XmStringCreateLtoR("Цвета", xh_charset),
                                    NULL);
            XmStringFree(s);
            
            grid = XhCreateGrid(frame, "grid");
            XhGridSetGrid   (grid, 0, 0);
            XhGridSetSpacing(grid, 0, 0);
            XhGridSetPadding(grid, 0, 0);
            
            for (row = 0, clp = colhelp_contents;
                 row < countof(colhelp_contents);
                 row++,   clp++)
            {
                fg = clp->fgcol; if (fg < 0) fg = XH_COLOR_FG_DEFAULT;
                bg = clp->bgcol; if (bg < 0) bg = XH_COLOR_BG_DEFAULT;
                
                /* A model... */
                w = XtVaCreateManagedWidget("rowlabel", xmLabelWidgetClass, grid,
                                            XmNlabelString, s = XmStringCreateLtoR(clp->label, xh_charset),
                                            XmNalignment,  XmALIGNMENT_BEGINNING,
                                            XmNforeground, XhGetColor(fg),
                                            XmNbackground, XhGetColor(bg),
                                            NULL);
                XmStringFree(s);
                
                XhGridSetChildPosition (w, 0,   row);
                XhGridSetChildFilling  (w, 1,   0);
                XhGridSetChildAlignment(w, -1, -1);
                
                /* ...and a comment */
                w = XtVaCreateManagedWidget("rowlabel", xmLabelWidgetClass, grid,
                                            XmNlabelString, s = XmStringCreateLtoR(clp->comment, xh_charset),
                                            XmNalignment,  XmALIGNMENT_BEGINNING,
                                            NULL);
                XmStringFree(s);
                
                XhGridSetChildPosition (w, 1,   row);
                XhGridSetChildFilling  (w, 0,   0);
                XhGridSetChildAlignment(w, -1, -1);
            }
        }

        /* List of key/mouse bindings */
        if (parts & CHL_HELP_KEYS)
        {
            frame = XtVaCreateManagedWidget("frame", xmFrameWidgetClass, form,
                                            XmNmarginWidth, 1,
                                            NULL);
            attachtop(frame, prev, 0);
            prev = frame;
            
            XtVaCreateManagedWidget("title", xmLabelWidgetClass, frame,
                                    XmNframeChildType, XmFRAME_TITLE_CHILD,
                                    XmNlabelString, s = XmStringCreateLtoR("Использование клавиатуры и мыши", xh_charset),
                                    NULL);
            XmStringFree(s);
            
            grid = XhCreateGrid(frame, "grid");
            XhGridSetGrid   (grid, 0, 0);
            XhGridSetSpacing(grid, 0, 0);
            XhGridSetPadding(grid, 0, 0);
            
            for (row = 0, klp = keyhelp_contents;
                 row < countof(keyhelp_contents);
                 row++,   klp++)
            {
                /* Have we reached keys/mouse-elem separator? */
                if (klp->keys[0] == '\0')
                {
                    if (parts & CHL_HELP_MOUSE) continue;
                    else                        break;
                }

                /* A model... */
                w = XtVaCreateManagedWidget("rowlabel", xmLabelWidgetClass, grid,
                                            XmNlabelString, s = XmStringCreateLtoR(klp->keys, xh_charset),
                                            XmNalignment,  XmALIGNMENT_END,
                                            NULL);
                XmStringFree(s);
                
                XhGridSetChildPosition (w, 0,   row);
                XhGridSetChildFilling  (w, 0,   0);
                XhGridSetChildAlignment(w, +1, -1);
                
                /* ...and a comment */
                w = XtVaCreateManagedWidget("rowlabel", xmLabelWidgetClass, grid,
                                            XmNlabelString, s = XmStringCreateLtoR(klp->comment, xh_charset),
                                            XmNalignment,  XmALIGNMENT_BEGINNING,
                                            NULL);
                XmStringFree(s);
                
                XhGridSetChildPosition (w, 1,   row);
                XhGridSetChildFilling  (w, 0,   0);
                XhGridSetChildAlignment(w, -1, -1);
            }
        }
        
        rec->initialized = 1;
    }

    if (XtIsManaged(rec->box)) 
        XhStdDlgHide(rec->box);
    else
        XhStdDlgShow(rec->box);
}
