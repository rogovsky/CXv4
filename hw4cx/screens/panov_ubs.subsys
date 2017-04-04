#

define(`PANOV_UBS_IDENT_s',
       ifelse(PANOV_UBS_IDENT, `',
       `mainGrouping',
       PANOV_UBS_IDENT))

define(`RGSWCH',
       `knob $1 $2 choicebs nolabel items:"#T$3\b\blit=red\t$4\b\blit=green" r:$5 w:"_all_code; qryval; putchan $7; \
                    push 1; qryval; sub; putchan $6;"')

define(`PANOV_STAT',
       `disp $1 $2 led "color=green,offcol=red,shape=default,victim=$3" r:$4 yelwrange:0.5-1.5')
define(`PANOV_SET_AND_MES',
       `knob $1s "" text "" $2 $3 r:set_$1 alwdrange:0-$4 step:$5
        disp $1m "" text "" $2 $3 r:mes_$1')

define(`PANOV_RANGE',
       `knob $1_min "["       text withlabel units:""  dpyfmt:$2 r:min_$1 step:$3
        knob $1_max "-"       text withlabel units:"]" dpyfmt:$2 r:max_$1 step:$3
        knob $1_clb "Калибр." button                             r:clb_$1
        disp $1_cst "/"       text           units:"%" dpyfmt:"%3.0f" r:cst_$1
        ')

define(`PANOV_ILK',
       `knob - $1 onoff color=amber            r:lkm_$2
        disp - "" led   color=red,shape=circle r:ilk_$2')

grouping main PANOV_UBS_IDENT_s "Main screen" grid nocoltitles,norowtitles \
        1 0 1 "" "" "" 5 {

    knob mode "" choicebs \
            items:"С\bСон\blit=blue\tТ\bТест\blit=yellow\tР\bРабота\blit=green\tО\bОтладка\blit=gray\t\nП\bПрограммирование\blit=brown\tА\bАвария\blit=red" \
            r:set_mode yelwrange:0-4

    container ctl "Controls" grid nodecor \
            param1:7 content:4 {
        container hwaddr - grid nodecor param1:2 content:2 {
            disp hi - text dpyfmt:"%1.0f" \
                    r:"_all_code; getchan hwaddr; div 16; trunc; ret"
            disp lo "" selector \
                    items:"#T0\t1\t2\t3\t4\t5\t6\t7\t8\t9\tA\tb\tC\td\tE\tF" \
                    r:"_all_code; getchan hwaddr; mod 16;        ret"
        }
#        knob power "" choicebs \
#                items:"#TOff\b\blit=red\tOn\b\blit=green" \
#                r:stat_ubs_power \
#                w:"_all_code; qryval; putchan power_on;
#                   push 1; qryval; sub; putchan power_off"
        RGSWCH(power,"Power",Выкл,Вкл,stat_ubs_power,power_off,power_on)
        button reboot_ubs "Перезагрузка" button r:reboot_ubs
        button -rst_ilks  "Сбр.блк"      button r:rst_ubs_ilk
    }

    noop - - hseparator layinfo:horz=fill

    container sam "SAM" grid nodecor \
            param1:8 content:24 {

        PANOV_STAT("st2_s", "БЗ2", "ST2_IHEATs", stat_st2_online)
        RGSWCH("heat_2", "Heat 2",Выкл,Вкл,stat_st2_heatin,chan_st2_heat_off,chan_st2_heat_on)
        noop - "Iнак2" hlabel layinfo:horz=right
        PANOV_SET_AND_MES(st2_iheat, "A", "%4.2f", 2.91, 0.01)
        disp st2iarcm "" text "" "mA" "%3.0f" r:mes_st2_iarc normrange:12.0-33.0
        noop - " " hlabel
        disp - "" led "color=red" tip:"Блокировка БЗ2" \
                r:"_all_code; getchan ilk_ubs_st2; getchan lkm_ubs_st2; mul; ret" \
                yelwrange:-0.5-+0.5

        PANOV_STAT("st1_s", "БЗ1", "ST1_IHEATs", stat_st1_online)
        RGSWCH("heat_1", "Heat 1",Выкл,Вкл,stat_st1_heatin,chan_st1_heat_off,chan_st1_heat_on)
        noop - "Iнак1" hlabel layinfo:horz=right
        PANOV_SET_AND_MES(st1_iheat, "A", "%4.2f", 2.91, 0.01)
        disp st1iarcm "" text "" "mA" "%3.0f" r:mes_st1_iarc normrange:12.0-33.0
        noop - " " hlabel
        disp - "" led "color=red" tip:"Блокировка БЗ1" \
                r:"_all_code; getchan ilk_ubs_st1; getchan lkm_ubs_st1; mul; ret" \
                yelwrange:-0.5-+0.5

        PANOV_STAT("dgs_s", "БРМ", "DGS_UDGSs", stat_dgs_online)
        RGSWCH("udgs",   "Udgs",  Выкл,Вкл,stat_dgs_is_on, chan_dgs_udgs_off,chan_dgs_udgs_on)
        noop - "Uрзм" hlabel layinfo:horz=right
        PANOV_SET_AND_MES(dgs_udgs,  "V", "%4.0f", 1200, 10)
        noop - " " hlabel
        noop - " " hlabel
        disp - "" led "color=red" tip:"Блокировка БРМ" \
                r:"_all_code; getchan ilk_ubs_dgs; getchan lkm_ubs_dgs; mul; ret" \
                yelwrange:-0.5-+0.5
    }

    container service "Service" grid \
            noshadow,nocoltitles,norowtitles,folded,fold_h \
            layinfo:horz=center \
            param1:1 content:2 {
        container ctl "Controls" grid nodecor param1:2 content:2 {
            container limits "Диапазоны und калибровка" grid \
                    nofold,nocoltitles str2:"Iнак2\tIнак1\tUразм" \
                    param1:4 content:12 {
                PANOV_RANGE(st2_iheat, "%4.2f", 0.01)
                PANOV_RANGE(st1_iheat, "%4.2f", 0.01)
                PANOV_RANGE(dgs_udgs,  "%4.0f", 10)
            }
            container misc "Управление" grid \
                    nofold,nocoltitles,norowtitles \
                    param1:1 content:3 {
                knob   tht_time "T прогр. нак."      text \
                        noincdec,withlabel "s"  "%3.0f" \
                        r:set_tht_time alwdrange:0-255
                button -        "Перечитать HWADDR"  button r:reread_hwaddr
                button -        "Перезагрузить всех" button r:reboot_all
            }
        }
                
        container ilk "Блокировки" grid \
                nofold,nocoltitles,norowtitles \
                param1:5 content:5 {

            container st1 "БЗ1" grid nodecor param1:1 content:3 {
                container ilk_ctl "Упр.блок." grid nodecor param1:3 content:3 {
                    knob - "БЗ1"   onoff  color=amber            r:lkm_ubs_st1
                    knob - "Сброс" button                        r:rst_st1_ilk
                    disp - ""      led    color=red,shape=circle r:ilk_ubs_st1 \
                            layinfo:horz=right
                } layinfo:horz=fill
                noop - - hseparator layinfo:horz=fill
                container ilocks "Блокировки" grid nodecor param1:2 content:16 {
                    PANOV_ILK("0 Нет блока",        st1_connect)
                    PANOV_ILK("1 Накал выкл.",      st1_heat_off)
                    PANOV_ILK("2 ...не прогрет",    st1_heat_cold)
                    PANOV_ILK("3 Iнак вне диап.",   st1_heat_range)
                    PANOV_ILK("4 Iдуги вне диап.",  st1_arc_range)
                    PANOV_ILK("5 Нет дуги",         st1_arc_fast)
                    PANOV_ILK("6 Нет высокого",     st1_high_fast)
                    PANOV_ILK("7 -резерв-",         st1_res7)
                }
            }

            noop - - vseparator layinfo:vert=fill

            container st2 "БЗ2" grid nodecor param1:1 content:3 {
                container ilk_ctl "Упр.блок." grid nodecor param1:3 content:3 {
                    knob - "БЗ2"   onoff  color=amber            r:lkm_ubs_st2
                    knob - "Сброс" button                        r:rst_st2_ilk
                    disp - ""      led    color=red,shape=circle r:ilk_ubs_st2 \
                            layinfo:horz=right
                } layinfo:horz=fill
                noop - - hseparator layinfo:horz=fill
                container ilocks "Блокировки" grid nodecor param1:2 content:16 {
                    PANOV_ILK("0 Нет блока",        st2_connect)
                    PANOV_ILK("1 Накал выкл.",      st2_heat_off)
                    PANOV_ILK("2 ...не прогрет",    st2_heat_cold)
                    PANOV_ILK("3 Iнак вне диап.",   st2_heat_range)
                    PANOV_ILK("4 Iдуги вне диап.",  st2_arc_range)
                    PANOV_ILK("5 Нет дуги",         st2_arc_fast)
                    PANOV_ILK("6 Нет высокого",     st2_high_fast)
                    PANOV_ILK("7 -резерв-",         st2_res7)
                }
            }

            noop - - vseparator layinfo:vert=fill

            container dgs "БРМ" grid nodecor param1:1 content:3 {
                container ilk_ctl "Упр.блок." grid nodecor param1:3 content:3 {
                    knob - "БРМ"   onoff  color=amber            r:lkm_ubs_dgs
                    knob - "Сброс" button                        r:rst_dgs_ilk
                    disp - ""      led    color=red,shape=circle r:ilk_ubs_dgs \
                            layinfo:horz=right
                } layinfo:horz=fill
                noop - - hseparator layinfo:horz=fill
                container ilocks "Блокировки" grid nodecor param1:2 content:16 {
                    PANOV_ILK("0 Нет блока",        dgs_connect)
                    PANOV_ILK("1 Uрзм выкл.",       dgs_udgs_off)
                    PANOV_ILK("2 -резерв-",         dgs_res2)
                    PANOV_ILK("3 Uрзм вне диап.",   dgs_udgs_range)
                    PANOV_ILK("4 -резерв-",         dgs_res4)
                    PANOV_ILK("5 Нет Iрзм",         dgs_dgs_fast)
                    PANOV_ILK("6 -резерв-",         dgs_res6)
                    PANOV_ILK("7 -резерв-",         dgs_res7)
                }
            }

        }
    }
}

wintitle main "Panov's UBS screen"
winname  main panov_ubs
winclass main PanovUBS
winopts  main ""

undefine(`PANOV_UBS_IDENT')
