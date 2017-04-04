#ifndef __XH_FALLBACKS_H
#define __XH_FALLBACKS_H


#ifndef XH_FONT_PROPORTIONAL_FMLY
  #define XH_FONT_PROPORTIONAL_FMLY "lucida"
#endif
#ifndef XH_FONT_FIXED_FMLY
  #define XH_FONT_FIXED_FMLY        "lucidatypewriter"
#endif

#ifndef XH_FONT_MEDIUM_WGHT
  #define XH_FONT_MEDIUM_WGHT       "medium"
#endif
#ifndef XH_FONT_BOLD_WGHT
  #define XH_FONT_BOLD_WGHT         "bold"
#endif

#ifndef XH_FONT_BASESIZE
  #define XH_FONT_BASESIZE "12"
#endif
#ifndef XH_FONT_TINYSIZE
  #define XH_FONT_TINYSIZE "10"
#endif
#ifndef XH_FONT_TICKSIZE
  #define XH_FONT_TICKSIZE "8"
#endif
#ifndef XH_FONT_MEDSIZE
  #define XH_FONT_MEDSIZE  "24"
#endif
#ifndef XH_FONT_HUGESIZE
  #define XH_FONT_HUGESIZE "96"
#endif

#define XH_PROPORTIONAL_FONT      "-*-" XH_FONT_PROPORTIONAL_FMLY "-" XH_FONT_MEDIUM_WGHT "-r-*-*-" XH_FONT_BASESIZE "-*-*-*-*-*-*-*"
#define XH_MINI_TITLES_FONT       "-*-" XH_FONT_PROPORTIONAL_FMLY "-" XH_FONT_MEDIUM_WGHT "-r-*-*-" XH_FONT_BASESIZE "-*-*-*-*-*-*-*"
#define XH_FIXED_FONT             "-*-" XH_FONT_FIXED_FMLY        "-" XH_FONT_MEDIUM_WGHT "-r-*-*-" XH_FONT_BASESIZE "-*-*-*-*-*-*-*"
#define XH_FIXED_BOLD_FONT        "-*-" XH_FONT_FIXED_FMLY        "-" XH_FONT_BOLD_WGHT   "-r-*-*-" XH_FONT_BASESIZE "-*-*-*-*-*-*-*"
#define XH_TINY_PROPORTIONAL_FONT "-*-" XH_FONT_PROPORTIONAL_FMLY "-" XH_FONT_MEDIUM_WGHT "-r-*-*-" XH_FONT_TINYSIZE "-*-*-*-*-*-*-*"
#define XH_TINY_FIXED_FONT        "-*-" XH_FONT_FIXED_FMLY        "-" XH_FONT_MEDIUM_WGHT "-r-*-*-" XH_FONT_TINYSIZE "-*-*-*-*-*-*-*"
#define XH_TINY_FIXED_BOLD_FONT   "-*-" XH_FONT_FIXED_FMLY        "-" XH_FONT_BOLD_WGHT   "-r-*-*-" XH_FONT_TINYSIZE "-*-*-*-*-*-*-*"
#define XH_TICK_PROPORTIONAL_FONT "-*-helvetica-medium-r-*-*-" XH_FONT_TICKSIZE "-*-*-*-*-*-*-*"
#define XH_TITLES_FONT            "-*-" XH_FONT_PROPORTIONAL_FMLY "-" XH_FONT_BOLD_WGHT   "-r-*-*-" XH_FONT_MEDSIZE  "-*-*-*-*-*-*-*"
#define XH_MINI_TITLES_FONT       "-*-" XH_FONT_PROPORTIONAL_FMLY "-" XH_FONT_BOLD_WGHT   "-r-*-*-" XH_FONT_BASESIZE "-*-*-*-*-*-*-*"
#define XH_HUGE_PROPORTIONAL_FONT "-*-*-bold-r-*-*-"   XH_FONT_HUGESIZE "-*-*-*-p-*-iso*-1"

#define XH_GENERAL_FALLBACKS \
    "*fontList:"                               XH_PROPORTIONAL_FONT, \
    "*font:"                                   XH_PROPORTIONAL_FONT, \
    "*.XmText.fontList:"                       XH_FIXED_FONT, \
    "*shadowThickness:"                        "2",         \
    "*fillOnArm:"                              "True",      \
    "*highlightThickness:"                     "1",         \
                                                            \
    "*XmMessageBox.Message.fontList:"          XH_TITLES_FONT, \
    "*XmText.shadowThickness:"                 "1",         \
    "*XmForm.shadowThickness:"                 "1",         \
    "*XmToggleButton.shadowThickness:"         "0",         \
                                                            \
    /* ???_FALLBACKS */                                     \
    "*workSpace.shadowThickness:"              "0",         \
                                                            \
    /* TOOLBAR_FALLBACKS */                                 \
    "*toolBar.shadowThickness:"                "0",         \
                                                            \
    "*toolLabel.fontList:"                     XH_TITLES_FONT, \
    "*toolLabel.marginHeight:"                 "2",         \
    "*toolLabel.marginTop:"                    "0",         \
    "*toolLabel.shadowThickness:"              "0",         \
                                                            \
    "*miniToolLabel.fontList:"                 XH_MINI_TITLES_FONT, \
    "*miniToolLabel.marginHeight:"             "3",         \
    "*miniToolLabel.marginTop:"                "0",         \
    "*miniToolLabel.shadowThickness:"          "0",         \
                                                            \
    "*toolBanner.fontList:"                    XH_TITLES_FONT, \
    "*toolBanner.marginHeight:"                "2",         \
                                                            \
    "*miniToolBanner.fontList:"                XH_MINI_TITLES_FONT, \
    "*miniToolBanner.marginHeight:"            "3",         \
                                                            \
    "*miniToolButton.shadowThickness:"         "2",         \
    "*miniToolButton.marginHeight:"            "1",         \
                                                            \
    /* STATUSLINE_FALLBACKS */                              \
    "*statusLineForm.shadowThickness:"         "0",         \
    "*statusLine.marginHeight:"                "0",         \
    "*statusLine.fontList:"                    XH_TINY_PROPORTIONAL_FONT

#define XH_DATA_FALLBACKS \
    "*.text_o.shadowThickness:"                "0",         \
    "*.text_o.marginWidth:"                    "3",         \
    "*.text_o.marginHeight:"                   "3"/*"3"*/,    /* =2(input.~) + 1 (input.shadow) */ \
    "*.text_o.columns:"                        "12",        \
    "*.text_i.marginWidth:"                    "3",         \
    "*.text_i.marginHeight:"                   "2"/*"2"*/,  \
    "*.text_i.columns:"                        "12",        \
                                                            \
    "*.onoff_i.marginWidth:"                  "0",         \
    "*.onoff_i.marginHeight:"                 "1"/*"3"*/,  \
    "*.onoff_i.marginLeft:"                   "0",         \
    "*.onoff_i.marginRight:"                  "0",         \
    "*.onoff_i.marginTop:"                    "2",         \
    "*.onoff_i.marginBottom:"                 "2",         \
    "*.onoff_i.shadowThickness:"              "0",         \
    "*.onoff_o.marginWidth:"                  "0",         \
    "*.onoff_o.marginHeight:"                 "1"/*"3"*/,  \
    "*.onoff_o.marginLeft:"                   "0",         \
    "*.onoff_o.marginRight:"                  "0",         \
    "*.onoff_o.marginTop:"                    "2",         \
    "*.onoff_o.marginBottom:"                 "2",         \
    "*.onoff_o.shadowThickness:"              "0",         \
                                                            \
    "*.light.marginWidth:"                     "3",         \
    "*.light.marginHeight:"                    "2"/*"2"*/,  \
                                                            \
    "*.submenuBtn.shadowThickness:"            "2",         \
    "*.submenuBtn.marginHeight:"               "2",         \
    "*.submenuBtn.highlightThickness:"         "0",         \
                                                            \
    "*.push_i.shadowThickness:"                "2",         \
    "*.push_i.marginHeight:"                   "1",         \
    "*.push_o.shadowThickness:"                "2",         \
    "*.push_o.marginHeight:"                   "1",         \
                                                            \
    "*.arrow_i.shadowThickness:"               "2",         \
    "*.arrow_i.detailShadowThickness:"         "2",         \
    "*.arrow_o.shadowThickness:"               "2",         \
    "*.arrow_o.detailShadowThickness:"         "2",         \
                                                            \
    "*.selector_i.marginWidth:"                  "0",       \
    "*.selector_i.marginHeight:"                 "0",       \
    "*.selector_i.OptionButton.shadowThickness:" "2",       \
    "*.selector_o.marginWidth:"                  "0",       \
    "*.selector_o.marginHeight:"                 "0",       \
    "*.selector_o.OptionButton.shadowThickness:" "2",       \
    "*.selectorPullDown.shadowThickness:"      "1",         \
    "*.selectorItem.marginHeight:"             "0",         \
    "*.selectorItem.shadowThickness:"          "1",         \
    "*.selector_i.OptionLabel.marginWidth:"      "0",       \
    "*.selector_i.OptionLabel.marginHeight:"     "3",       \
    "*.selector_i.OptionLabel.marginLeft:"       "0",       \
    "*.selector_i.OptionLabel.marginRight:"      "0",       \
    "*.selector_i.OptionLabel.marginTop:"        "0",       \
    "*.selector_i.OptionLabel.marginBottom:"     "0",       \
    "*.selector_i.OptionLabel.shadowThickness:"  "0",       \
    "*.selector_o.OptionLabel.marginWidth:"      "0",       \
    "*.selector_o.OptionLabel.marginHeight:"     "3",       \
    "*.selector_o.OptionLabel.marginLeft:"       "0",       \
    "*.selector_o.OptionLabel.marginRight:"      "0",       \
    "*.selector_o.OptionLabel.marginTop:"        "0",       \
    "*.selector_o.OptionLabel.marginBottom:"     "0",       \
    "*.selector_o.OptionLabel.shadowThickness:"  "0",       \
                                                            \
    "*.dial.shadowThickness:"                  "0",         \
    "*.dialButton.shadowThickness:"            "0",         \
    "*.dial.text_o.fontList:"                  XH_TINY_FIXED_FONT, \
    "*.dial.text_i.fontList:"                  XH_TINY_FIXED_FONT, \
    "*.dialTitle.fontList:"                    XH_TINY_PROPORTIONAL_FONT, \
    "*.dialDial.tickFontList:"                 XH_TICK_PROPORTIONAL_FONT, \
                                                            \
    "*.elemCaption.marginWidth:"               "0",         \
    "*.elemCaption.marginHeight:"              "3"/*"3"*/,  \
    "*.elemCaption.marginLeft:"                "0",         \
    "*.elemCaption.marginRight:"               "0",         \
    "*.elemCaption.marginTop:"                 "0",         \
    "*.elemCaption.marginBottom:"              "0",         \
    "*.elemCaption.shadowThickness:"           "0",         \
                                                            \
    "*.rowlabel.marginWidth:"                  "0",         \
    "*.rowlabel.marginHeight:"                 "3"/*"3"*/,  \
    "*.rowlabel.marginLeft:"                   "0",         \
    "*.rowlabel.marginRight:"                  "0",         \
    "*.rowlabel.marginTop:"                    "0",         \
    "*.rowlabel.marginBottom:"                 "0",         \
    "*.rowlabel.shadowThickness:"              "0",         \
                                                            \
    "*.collabel.marginWidth:"                  "0",         \
    "*.collabel.marginHeight:"                 "3"/*"3"*/,  \
    "*.collabel.marginLeft:"                   "0",         \
    "*.collabel.marginRight:"                  "0",         \
    "*.collabel.marginTop:"                    "0",         \
    "*.collabel.marginBottom:"                 "0",         \
    "*.collabel.shadowThickness:"              "0",         \
                                                            \
    "*.hsep.separatorType:"                    "SHADOW_ETCHED_IN_DASH", \
    "*.vsep.separatorType:"                    "SHADOW_ETCHED_IN_DASH", \
    "*.hsep.height:"                           "2",         \
    "*.vsep.width:"                            "2",         \
                                                            \
    "*.rflagLed.fontList:"                     XH_TINY_FIXED_FONT, \
    "*.rflagLed.shadowThickness:"              "0",         \
    "*.rflagLed.marginWidth:"                  "0",         \
                                                            \
    "*.bigvBox*rowlabel.fontList:"             XH_TITLES_FONT, \
    "*.bigvBox*text_o.fontList:"               XH_HUGE_PROPORTIONAL_FONT, \
    "*.bigvBox*okLabelString:"                 "Close"

#define XH_FILE_FALLBACKS \
    "*.pathMode:"                              "PATH_MODE_RELATIVE", \
    "*.loadModeDialog.dialogTitle:"            "Загрузить режим",  \
    "*.saveModeDialog.dialogTitle:"            "Сохранить режим",  \
    "*.loadDataDialog.dialogTitle:"            "Загрузить данные", \
    "*.saveDataDialog.dialogTitle:"            "Сохранить данные", \
    "*.filterLabelString:"                     "Фильтр",           \
    "*.dirTextLabelString:"                    "Директория",       \
    "*.dirListLabelString:"                    "Директории",       \
    "*.fileListLabelString:"                   "Файлы",            \
    "*.selectionLabelString:"                  "Имя",              \
    "*.cancelLabelString:"                     "Отмена",           \
    "*.applyLabelString:"                      "Фильтровать",      \
    "*.showFiles.labelString:"                 "..."

#define XH_STANDARD_FALLBACKS \
    XH_GENERAL_FALLBACKS, \
    XH_DATA_FALLBACKS,    \
    XH_FILE_FALLBACKS,    \
    NULL


#endif /* __XH_FALLBACKS_H */
