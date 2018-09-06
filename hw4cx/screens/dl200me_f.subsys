     define(`forloop',
            `pushdef(`$1', `$2')_forloop(`$1', `$2', `$3', `$4')popdef(`$1')')
     define(`_forloop',
            `$4`'ifelse($1, `$3', ,
                   `define(`$1', incr($1))_forloop(`$1', `$2', `$3', `$4')')')

#---------------------------------------------------------------------

define(`DL200ME_F_NUMOUTPUTS', 16)

define(`DL200ME_F_T_LINE', `
    knob off`'eval($1+1) "" onoff "color=black" r:off$1
    knob set`'eval($1+1) "" text  ""            r:t$1    dpyfmt:"%6.0f"
    disp ilk`'eval($1+1) "" led   "color=red"   r:ailk$1
')

define(`DL200ME_F_I_LINE', `
    knob iinh`'eval($1+1) "" onoff "color=amber" r:iinh$1
    knob ilas`'eval($1+1) "" onoff "color=blue"  r:ilas$1
')

define(`DL200ME_F_TITLE_s',
       ifdef(DL200ME_F_TITLE, DL200ME_F_TITLE, `"Fast DL200ME"'))
define(`DL200ME_F_ROWTITLES_s',
       ifdef(DL200ME_F_ROWTITLES, DL200ME_F_ROWTITLES,
       `"forloop(`N', 1, DL200ME_F_NUMOUTPUTS, `N\t')"'))

grouping main DL200ME_F DL200ME_F_TITLE_s \
         grid "nocoltitles,norowtitles" param3:2 content:5 {

    disp -    "#" text "withlabel" r:serial   dpyfmt:"%-3.0f" tip:"Serial #" 
    knob mode ""  selector         r:run_mode items:"Ext\tProg"

    container "" "" grid "noshadow,notitle,nocoltitles,norowtitles" \
            2 content:2 {
        knob tyk  ""       arrowrt "offcol=blue size=giant"  r:do_shot
        container "" "" grid "noshadow,notitle,nocoltitles,norowtitles" \
                1 content:2 {
            container "" "" grid "noshadow,notitle,nocoltitles,norowtitles" \
                    2 content:2 {
                knob -auto "каждые" onoff   "color=green" r:auto_shot
                knob secs  ""       text    ""            r:auto_msec \
                        units:"s" dpyfmt:"%4.1f"
            }
            container "" "" grid "noshadow,notitle,nocoltitles,norowtitles" \
                    5 content:5 {
                disp - "  !  " led   "panel"       r:was_shot
                knob - "-All"  onoff "color=black" r:alloff
                disp - ""      led   "color=green" r:was_fin   tip:"Было срабатывание"
                disp - ""      led   "color=red"   r:was_ilk   tip:"Была блокировка"
                disp - ""      text  ""            r:auto_left dpyfmt:"%6.0f" \
                                                               tip:"Осталось до авто-тука"
            }
        }
    }

    noop - - hseparator layinfo:horz=fill

    container "" "" grid "noshadow,notitle,nocoltitles,norowtitles" \
            3 content:3 {

        container "" "" grid "noshadow,notitle" \
                str1:"-\bОтключение\tT, ns\t?\bБлокировка?" \
                str2:DL200ME_F_ROWTITLES_s \
                param1:3 content:eval(3*DL200ME_F_NUMOUTPUTS) {
            forloop(`N', 0, eval(DL200ME_F_NUMOUTPUTS-1), `DL200ME_F_T_LINE(N)')
        }

        noop - - vseparator layinfo:vert=fill

        container "" "" grid "noshadow,notitle" \
                str1:"\\\bЗапрет блокировки\t!\b\"Блокировка/Авария-S\"" \
                str2:"forloop(`N', 1, DL200ME_F_NUMOUTPUTS, `N\t')" \
                param1:2 content:eval(2*DL200ME_F_NUMOUTPUTS) {
            forloop(`N', 0, eval(DL200ME_F_NUMOUTPUTS-1), `DL200ME_F_I_LINE(N)')
        }
    }
}

undefine(`DL200ME_F_TITLE')
undefine(`DL200ME_F_ROWTITLES')

wintitle main "Fast DL200ME"
winname  main dl200me_f
winclass main DL200ME_F
winopts  main ""
