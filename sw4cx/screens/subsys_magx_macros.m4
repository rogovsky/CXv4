
# 1:c1 2:c2
define(`MAGX_C100',
       `"_all_code;
         getchan $1; cmp_if_eq 0; ret 0;
         getchan $1; getchan $2; div; sub 1; abs; mul 100; ret"')

# 1:c1 2:c2 3:c_s
define(`MAGX_C100_OPR',
       `"_all_code;
         getchan $3; cmp_if_eq 0; ret 0;
         getchan $1; cmp_if_eq 0; ret 0;
         getchan $1; getchan $2; div; sub 1; abs; mul 100; ret"')

# 1:c1 2:c2 3:c_s
define(`MAGX_CFWEIRD100',
       `"_all_code;
         getchan $3; cmp_if_eq 0; ret 0;
         getchan $1; cmp_if_eq 0; ret 0;
         getchan $1; getchan $2; div; sub 1; abs; mul 100;
         dup; div 1.0 ifelse(comment: "1.0" is 1%); trunc; weird;
         ret"')

# 1:id 2:label 3:"Off" 4:"On" 5:c_state 5:c_off 6:c_on
define(`RGSWCH',
       `knob/ign_otherop $1 $2 choicebs nolabel items:"#T$3\b\blit=red\t$4\b\blit=green" \
               r:$5 w:"_all_code; qryval; putchan $7; \
               push 1; qryval; sub; putchan $6; ret;"')

# 1:id&r 2:label 3:tip 4:max 5:step 6:(a40&d16)base 7:N(0-7)
define(`MAGX_VCH300_LINE',
       `knob/groupable $1_set $2   text dpyfmt:"%6.1f" r:"$1.Iset" \
                alwdrange:0-$4 options:nounits tip:$3
        disp           $1_mes $2_m text dpyfmt:"%6.1f" r:"$1.Imes" \
                normrange:0-1 yelwrange:0-2 disprange:0-$4 \
                c:MAGX_C100_OPR($1.Iset, $1.Imes, $1.opr)
        minmax         $1_dvn ""   text dpyfmt:"%6.3f" r:"$1.Imes"
        disp           $1_opr ""   led  shape=circle   r:"$1.opr"
        RGSWCH(-$1_swc, "", 0, 1, $1.is_on, $1.switch_off, $1.switch_on)
        container -$1_ctl "..." subwin str3:$2": extended controls" \
                content:1 {
            container "" "" grid nodecor 3 content:3 {
                container v3h "VCh300" grid noshadow,nocoltitles,norowtitles \
                        base:$1 param3:4 content:eval(4+3) {
                    disp   opr ""   led  shape=circle   r:opr
                    RGSWCH(onoff, "", Off, On, is_on, switch_off, switch_on)
                    disp   v3h_state "I_S" text dpyfmt:"%3.0f" r:v3h_state
                    button rst       "R"   button              r:reset_state
                    container ctl "Control" grid notitle,noshadow,norowtitles \
                            3 content:3 {
                        knob dac    "Set, A"    dpyfmt:"%7.1f" r:Iset      alwdrange:0-$4
                        knob dacspd "MaxSpd, A" dpyfmt:"%7.1f" r:Iset_rate disprange:-5000-+5000
                        disp daccur "Cur, A"    dpyfmt:"%7.1f" r:Iset_cur  disprange:-2500-+2500
                    }
                    noop - - hseparator layinfo:horz=fill
                    container "" "" grid nodecor 5 content:5 {
                        container "mes" "Measurements" grid noshadow,nocoltitles \
                                content:5 {
                            disp dms "DAC control"   text - "A" "%9.1f" r:dac_mes
                            disp sts "DCCT2"         text - "A" "%9.1f" r:dcct2
                            disp mes "DCCT1 (Imes)"  text - "A" "%9.1f" r:dcct1
                            disp u1  "U1"            text - "V" "%9.1f" r:u1mes
                            disp u2  "U2"            text - "V" "%9.6f" r:u2mes
                        }
                        noop - - vseparator layinfo:vert=fill
                        disp ilk   "Ilk" led "shape=circle,color=red"   r:ilk   yelwrange:-0.1-+0.1
                        disp c_ilk ""    led "shape=circle,color=amber" r:c_ilk
                        button rci "R" button                         r:reset_c_ilks
                    }
                }
                noop - - vseparator layinfo:vert=fill
                container c40d16 "CANADC40+CANDAC16" grid noshadow,nocoltitles,norowtitles \
                        base:$1.a40d16 content:3 {
                    container dac "DAC" grid notitle,noshadow,norowtitles \
                            3 content:3 {
                        disp out      "Set, V"      text dpyfmt:"%7.4f" r:out
                        disp out_rate "MaxSpd, V/s" text dpyfmt:"%7.4f" r:out_rate
                        disp out_cur  "Cur, V"      text dpyfmt:"%7.4f" r:out_cur
                    }
                    noop - - hseparator layinfo:horz=fill
                    container ":" "" grid nodecor \
                            3 content:3 {
                        container mes "ADC, V" grid noshadow,nocoltitles \
                                content:5 {
                            disp adc0 "+0"   dpyfmt:"%9.6f" r:adc0 disprange:-10-+10
                            disp adc1 "+1"   dpyfmt:"%9.6f" r:adc1 disprange:-10-+10
                            disp adc2 "+2"   dpyfmt:"%9.6f" r:adc2 disprange:-10-+10
                            disp adc3 "+3"   dpyfmt:"%9.6f" r:adc3 disprange:-10-+10
                            disp adc4 "+4"   dpyfmt:"%9.6f" r:adc4 disprange:-10-+10
                        }
                        noop - - vseparator layinfo:vert=fill
                        container io "I/O" grid noshadow,norowtitles,transposed \
                                str1:"DAC\tADC" \
                                param1:2 content:4 {
                            disp i0 "" led shape=circle            r:i0
                            disp i1 "" led shape=circle            r:i1
                            disp o0 "" led shape=circle,color=blue r:o0
                            disp o1 "" led shape=circle,color=blue r:o1
                        }
                    }
                }
            }
        }
        ')


define(`MAGX_V1K_CDAC20_ILK_LINE',
       `disp $1 $2 led "shape=circle,color=red"   r:ilk_$1  yelwrange:-0.1-+0.1
        disp -  "" led "shape=circle,color=amber" r:c_ilk_$1')

# 1:id&r 2:label 3:tip 4:max 5:step 6:c20_base
define(`MAGX_V1K_CDAC20_LINE',
       `knob/groupable $1_set $2   text dpyfmt:"%6.1f" r:"$1.Iset" \
                alwdrange:0-$4 options:nounits tip:$3
        disp           $1_mes $2_m text dpyfmt:"%6.1f" r:"$1.current1m" \
                normrange:0-1 yelwrange:0-2 disprange:0-$4 \
                c:MAGX_C100_OPR($1.Iset, $1.current1m, $1.opr)
        minmax         $1_dvn ""   text dpyfmt:"%6.3f" r:"$1.current1m"
        disp           $1_opr ""   led  shape=circle   r:"$1.opr"
        RGSWCH(-$1_swc, "", 0, 1, $1.is_on, $1.switch_off, $1.switch_on)
        container -$1_ctl "..." subwin str3:$2": extended controls" \
                content:1 {
            container "" "" grid nodecor 3 content:3 {
                container v1k "V1000" grid noshadow,nocoltitles,norowtitles \
                        base:$1 param3:4 content:eval(4+3+2) {
                    disp   opr ""   led  shape=circle   r:opr
                    RGSWCH(onoff, "", Off, On, is_on, switch_off, switch_on)
                    disp   v1k_state "I_S" text dpyfmt:"%3.0f" r:v1k_state
                    button rst       "R"   button              r:reset_state
                    container ctl "Control" grid notitle,noshadow,norowtitles \
                            3 content:3 {
                        knob dac    "Set, A"    dpyfmt:"%7.1f" r:Iset      alwdrange:0-2500
                        knob dacspd "MaxSpd, A" dpyfmt:"%7.1f" r:Iset_rate disprange:-2500-+2500
                        disp daccur "Cur, A"    dpyfmt:"%7.1f" r:Iset_cur  disprange:-2500-+2500
                    }
                    noop - - hseparator layinfo:horz=fill
                    container "" "" grid nodecor 3 content:3 {
                        container "mes" "Measurements" grid noshadow,nocoltitles \
                                content:8 {
                            disp current1m    "Current1M"    text - "A" "%9.1f" r:current1m 
                            disp voltage2m    "Voltage2M"    text - "A" "%9.1f" r:voltage2m  
                            disp current2m    "Current2M"    text - "A" "%9.1f" r:current2m  
                            disp outvoltage1m "OutVoltage1M" text - "V" "%9.1f" r:outvoltage1m
                            disp outvoltage2m "OutVoltage2M" text - "V" "%9.6f" r:outvoltage2m
                            disp adcdac       "DAC@ADC"      text - "A" "%9.1f" r:adc_dac      disprange:-2500-+2500
                            disp 0v           "0V"           text - "V" "%9.6f" r:adc_0v       disprange:-10-+10
                            disp 10v          "10V"          text - "V" "%9.6f" r:adc_p10v     disprange:-10-+10
                        }
                        noop - - vseparator layinfo:vert=fill
                        container "ilk" "Interlocks" grid \
                                noshadow,nocoltitles,norowtitles \
                                2 content:10 {
                            MAGX_V1K_CDAC20_ILK_LINE("overheat", "Overheating source")
                            MAGX_V1K_CDAC20_ILK_LINE("emergsht", "Emergency shutdown")
                            MAGX_V1K_CDAC20_ILK_LINE("currprot", "Current protection")
                            MAGX_V1K_CDAC20_ILK_LINE("loadovrh", "Load overheat/water")
                            button rst "Reset" button r:rst_ilks
                            button rci "R"     button r:reset_c_ilks
                        }
                    }
                    noop - - hseparator layinfo:horz=fill
                    container "" "" grid nodecor 3 content:3 {
                        button calb    "Clbr DAC" button r:do_calb_dac
                        knob   dc_mode "Dig.corr" onoff  r:digcorr_mode
                        disp   dc_val  ""         text dpyfmt:"%-8.0f" r:digcorr_factor
                    }
                }
                noop - - vseparator layinfo:vert=fill
                container adc "CDAC20" grid noshadow,nocoltitles,norowtitles \
                        base:$6 content:3 {
                    container dac "DAC" grid notitle,noshadow,norowtitles \
                            3 content:3 {
                        disp dac    "Set, V"      text dpyfmt:"%7.4f" r:out
                        disp dacspd "MaxSpd, V/s" text dpyfmt:"%7.4f" r:out_rate
                        disp daccur "Cur, V"      text dpyfmt:"%7.4f" r:out_cur
                    }
                    noop - - hseparator layinfo:horz=fill
                    container ":" "" grid nodecor \
                            3 content:3 {
                        container mes "ADC, V" grid noshadow,nocoltitles \
                                content:8 {
                            disp adc0 "0"   dpyfmt:"%9.6f" r:adc0 disprange:-10-+10
                            disp adc1 "1"   dpyfmt:"%9.6f" r:adc1 disprange:-10-+10
                            disp adc2 "2"   dpyfmt:"%9.6f" r:adc2 disprange:-10-+10
                            disp adc3 "3"   dpyfmt:"%9.6f" r:adc3 disprange:-10-+10
                            disp adc4 "4"   dpyfmt:"%9.6f" r:adc4 disprange:-10-+10
                            disp adc5 "DAC" dpyfmt:"%9.6f" r:adc5 disprange:-10-+10
                            disp adc6 "0V"  dpyfmt:"%9.6f" r:adc6 disprange:-10-+10
                            disp adc7 "10V" dpyfmt:"%9.6f" r:adc7 disprange:-10-+10
                        }
                        noop - - vseparator layinfo:vert=fill
                        container io "I/O" grid noshadow,norowtitles,transposed \
                                str1:"0\t1\t2\t3\t4\t5\t6\t7" \
                                param1:8 content:16 {
                            disp i0 "" led shape=circle            r:inprb0
                            disp i1 "" led shape=circle            r:inprb1
                            disp i2 "" led shape=circle            r:inprb2
                            disp i3 "" led shape=circle            r:inprb3
                            disp i4 "" led shape=circle            r:inprb4
                            disp i5 "" led shape=circle            r:inprb5
                            disp i6 "" led shape=circle            r:inprb6
                            disp i7 "" led shape=circle            r:inprb7
                            disp o0 "" led shape=circle,color=blue r:outrb0
                            disp o1 "" led shape=circle,color=blue r:outrb1
                            disp o2 "" led shape=circle,color=blue r:outrb2
                            disp o3 "" led shape=circle,color=blue r:outrb3
                            disp o4 "" led shape=circle,color=blue r:outrb4
                            disp o5 "" led shape=circle,color=blue r:outrb5
                            disp o6 "" led shape=circle,color=blue r:outrb6
                            disp o7 "" led shape=circle,color=blue r:outrb7
                        }
                    }
                }
            }
        }
        ')


define(`MAGX_IST_CDAC20_ILK_LINE',
       `disp $1 $2 led "shape=circle,color=red"   r:ilk_$1  yelwrange:-0.1-+0.1
        disp -  "" led "shape=circle,color=amber" r:c_ilk_$1')

# 1:id&r 2:label 3:tip 4:max 5:step 6:c20_base 7:weird_dcct2 8:reversable 9/opt:1=>veremeenko 10/opt:1=>bipolar
define(`MAGX_IST_CDAC20_LINE',
       `knob/groupable $1_set $2   text dpyfmt:"%6.1f" r:"$1.Iset" \
                alwdrange:ifelse(index($8$10,1),-1,0,-$4)-$4 \
                options:nounits tip:$3
        disp           $1_mes $2_m text dpyfmt:"%6.1f" r:"$1.dcct1" \
                normrange:0-1 yelwrange:0-2 disprange:ifelse(index($8$10,1),-1,0,-$4)-$4 \
                c:MAGX_C100_OPR($1.Iset, $1.dcct1, $1.opr)
        minmax         $1_dvn ""   text dpyfmt:"%6.3f" r:"$1.dcct1"
        disp           $1_opr ""   led  shape=circle   r:"$1.opr" \
                c:"_all_code; getchan $6.outrb1; getchan $1.opr;
                  sub; dup; weird; ret"
        RGSWCH(-$1_swc, "", 0, 1, $1.is_on, $1.switch_off, $1.switch_on)
        container -$1_ctl "..." subwin str3:$2": extended controls" \
                content:1 {
            container "" "" grid nodecor 3 content:3 {
                container ist "IST" grid noshadow,nocoltitles,norowtitles \
                        base:$1 param3:4 content:eval(4+4+1) {
                    disp   opr ""   led  shape=circle   r:opr
                    RGSWCH(onoff, "", Off, On, is_on, switch_off, switch_on)
                    disp   ist_state "I_S" text dpyfmt:"%3.0f" r:ist_state
                    button rst       "R"   button              r:reset_state
                    container ctl "Control" grid notitle,noshadow,norowtitles \
                            3 content:3 {
                        knob dac    "Set, A"    dpyfmt:"%7.1f" r:Iset      alwdrange:ifelse(index($8$10,1),-1,0,-$4)-$4
                        knob dacspd "MaxSpd, A" dpyfmt:"%7.1f" r:Iset_rate disprange:-$4-+$4
                        disp daccur "Cur, A"    dpyfmt:"%7.1f" r:Iset_cur  disprange:-$4-+$4
                    }
                    noop - - hseparator layinfo:horz=fill
                    container "" "" grid nodecor 3 content:3 {
                        container "mes" "Measurements" grid noshadow,nocoltitles \
                                content:8 {
                            disp dcct1  "DCCT1"   text - "A" "%9.1f" r:dcct1      disprange:-2500-+2500
                            disp dcct2  "DCCT2"   text - "A" "%9.1f" r:dcct2      disprange:-2500-+2500 \
                                    ifelse($7, 1, c:MAGX_CFWEIRD100(dcct1,dcct2,Iset_cur))
                            disp dacmes "DAC"     text - "A" "%9.1f" r:dac_mes    disprange:-2500-+2500
                        ifelse($9, 1, `
                            disp u2     "Uout-"   text - "V" "%9.1f" r:uout_minus disprange:-10-+10
                            disp adc4   "Uout+"   text - "V" "%9.6f" r:uout_plus  disprange:-10-+10
                        ', `
                            disp u2     "U2"      text - "V" "%9.1f" r:u2         disprange:-10-+10
                            disp adc4   "ADC4"    text - "V" "%9.6f" r:adc4       disprange:-10-+10
                        ')
                            disp adcdac "DAC@ADC" text - "A" "%9.1f" r:adc_dac    disprange:-2500-+2500
                            disp 0v     "0V"      text - "V" "%9.6f" r:adc_0v     disprange:-10-+10
                            disp 10v    "10V"     text - "V" "%9.6f" r:adc_p10v   disprange:-10-+10
                        }
                        noop - - vseparator layinfo:vert=fill
                        container "ilk" "Interlocks" grid \
                                noshadow,nocoltitles,norowtitles \
                                2 content:16 {
                            button rst "Reset" button r:rst_ilks
                            button rci "R"     button r:reset_c_ilks
                        ifelse($9, 1, `
                            MAGX_IST_CDAC20_ILK_LINE("out_prot1", "Out Prot 1")
                            MAGX_IST_CDAC20_ILK_LINE("out_prot2", "Out Prot 2")
                            MAGX_IST_CDAC20_ILK_LINE("temp",      "Temp")
                            MAGX_IST_CDAC20_ILK_LINE("inverter",  "Inverter")
                            MAGX_IST_CDAC20_ILK_LINE("phase",     "Phase")
                            MAGX_IST_CDAC20_ILK_LINE("out_prot3", "OutProt 3")
                            MAGX_IST_CDAC20_ILK_LINE("imax",      "Imax")
                        ', `
                            MAGX_IST_CDAC20_ILK_LINE("out_prot",  "Out Prot")
                            MAGX_IST_CDAC20_ILK_LINE("water",     "Water")
                            MAGX_IST_CDAC20_ILK_LINE("imax",      "Imax")
                            MAGX_IST_CDAC20_ILK_LINE("umax",      "Umax")
                            MAGX_IST_CDAC20_ILK_LINE("battery",   "Battery")
                            MAGX_IST_CDAC20_ILK_LINE("phase",     "Phase")
                            MAGX_IST_CDAC20_ILK_LINE("temp",      "Tempr")
                        ')
                        }
                    }
                    noop - - hseparator layinfo:horz=fill
                    container "" "" grid nodecor 4 content:4 {
                        button calb    "Clbr DAC" button r:do_calb_dac
                        knob   dc_mode "Dig.corr" onoff  r:digcorr_mode
                        disp   dc_val  ""         text dpyfmt:"%-8.0f" r:digcorr_factor
                        disp   sign    "S:"       text withlabel dpyfmt:"%+2.0f" r:cur_polarity
                    }
                }
                noop - - vseparator layinfo:vert=fill
                container adc "CDAC20" grid noshadow,nocoltitles,norowtitles \
                        base:$6 content:3 {
                    container dac "DAC" grid notitle,noshadow,norowtitles \
                            3 content:3 {
                        disp dac    "Set, V"      text dpyfmt:"%7.4f" r:out
                        disp dacspd "MaxSpd, V/s" text dpyfmt:"%7.4f" r:out_rate
                        disp daccur "Cur, V"      text dpyfmt:"%7.4f" r:out_cur
                    }
                    noop - - hseparator layinfo:horz=fill
                    container ":" "" grid nodecor \
                            3 content:3 {
                        container mes "ADC, V" grid noshadow,nocoltitles \
                                content:8 {
                            disp adc0 "0"   dpyfmt:"%9.6f" r:adc0 disprange:-10-+10
                            disp adc1 "1"   dpyfmt:"%9.6f" r:adc1 disprange:-10-+10
                            disp adc2 "2"   dpyfmt:"%9.6f" r:adc2 disprange:-10-+10
                            disp adc3 "3"   dpyfmt:"%9.6f" r:adc3 disprange:-10-+10
                            disp adc4 "4"   dpyfmt:"%9.6f" r:adc4 disprange:-10-+10
                            disp adc5 "DAC" dpyfmt:"%9.6f" r:adc5 disprange:-10-+10
                            disp adc6 "0V"  dpyfmt:"%9.6f" r:adc6 disprange:-10-+10
                            disp adc7 "10V" dpyfmt:"%9.6f" r:adc7 disprange:-10-+10
                        }
                        noop - - vseparator layinfo:vert=fill
                        container io "I/O" grid noshadow,norowtitles,transposed \
                                str1:"0\t1\t2\t3\t4\t5\t6\t7" \
                                param1:8 content:16 {
                            disp i0 "" led shape=circle            r:inprb0
                            disp i1 "" led shape=circle            r:inprb1
                            disp i2 "" led shape=circle            r:inprb2
                            disp i3 "" led shape=circle            r:inprb3
                            disp i4 "" led shape=circle            r:inprb4
                            disp i5 "" led shape=circle            r:inprb5
                            disp i6 "" led shape=circle            r:inprb6
                            disp i7 "" led shape=circle            r:inprb7
                            disp o0 "" led shape=circle,color=blue r:outrb0
                            disp o1 "" led shape=circle,color=blue r:outrb1
                            disp o2 "" led shape=circle,color=blue r:outrb2
                            disp o3 "" led shape=circle,color=blue r:outrb3
                            disp o4 "" led shape=circle,color=blue r:outrb4
                            disp o5 "" led shape=circle,color=blue r:outrb5
                            disp o6 "" led shape=circle,color=blue r:outrb6
                            disp o7 "" led shape=circle,color=blue r:outrb7
                        }
                    }
                }
            }
        }
        ')

# 1:id&r 2:label 3:tip 4:max 5:step 6:c124_base 7/opt:1=positive_only 8/opt:1=1_bit_status
define(`MAGX_MPS20_CEAC124_LINE',
       `knob/groupable $1_set $2   text dpyfmt:"%6.3f" r:"$1.Iset" \
                alwdrange:ifelse($7,1,0,-$4)-$4 \
                options:nounits tip:$3
        disp           $1_mes $2_m text dpyfmt:"%6.3f" r:"$1.Imes" \
                normrange:0-1 yelwrange:0-2 disprange:ifelse($7,1,0,-$4)-$4 \
                c:MAGX_C100_OPR($1.Iset, $1.Imes, $1.opr_s1)
        minmax         $1_dvn ""   text dpyfmt:"%6.3f" r:"$1.Imes"
        disp           $1_opr ""   led  shape=circle \
                r:"_all_code; getchan $1.opr_s1; ifelse($8,1,,`getchan $1.opr_s2; mul;') ret"
        RGSWCH(-$1_swc, "", 0, 1, $1.is_on, $1.switch_off, $1.switch_on)
        container -$1_ctl "..." subwin str3:$2": extended controls" \
                content:1 {
            container "" "" grid nodecor 3 content:3 {
                container mps20 "MPS-20" grid noshadow,nocoltitles,norowtitles \
                        base:$1 param3:5 content:eval(5+3) {
                    disp   opr                  "S1"  led shape=circle r:opr_s1
                    disp   opr ifelse($8,1,"-2","S2") led shape=circle r:opr_s2
                    RGSWCH(onoff, "", Off, On, is_on, switch_off, switch_on)
                    disp   mps20_state "M_S" text dpyfmt:"%3.0f" r:mps20_state
                    button rst         "R"   button              r:reset_state
                    container ctl "Control" grid notitle,noshadow,norowtitles \
                            3 content:3 {
                        knob dac    "Set, A"    dpyfmt:"%6.3f" r:Iset      \
                                alwdrange:ifelse($7,1,0,-$4)-$4
                        knob dacspd "MaxSpd, A" dpyfmt:"%6.3f" r:Iset_rate disprange:-5000-+5000
                        disp daccur "Cur, A"    dpyfmt:"%6.3f" r:Iset_cur  disprange:-2500-+2500
                    }
                    noop - - hseparator layinfo:horz=fill
                    container "" "" grid nodecor 6 content:6 {
                        container "mes" "Measurements" grid noshadow,nocoltitles \
                                content:2 {
                            disp   imes    "Imes"    text "" A %9.3f Imes
                            disp   umes    "Umes"    text "" V %9.3f Umes
                        }
                        noop - - vseparator layinfo:vert=fill
                        disp ilk "Ilk" led "shape=circle,color=red"   r:ilk   yelwrange:-0.1-+0.1
                        disp -   ""    led "shape=circle,color=amber" r:c_ilk
                        button rci "R" button                         r:reset_c_ilks
                        knob "" "" choicebs nolabel,compact r:disable \
                                items:"#TEnable\b\blit=green\tDisable\b\blit=red"
                    }
                }
                noop - - vseparator layinfo:vert=fill
                noop
            }
        }
        ')

# 1:id&r 2:label 3/opt:Iset_aux_params
define(`MAGX_COR4016_LINE',
       `knob/groupable $1_Iset $2 text nounits - %5.0f $1.Iset \
                alwdrange:-9000-+9000 step:100 $3
        disp           $1_Imes "" text -       - %5.0f $1.Imes \
                normrange:0-5 yelwrange:0-7 \
                c:MAGX_C100($1`'.Iset, $1`'.Imes) colz:important
        disp           $1_Umes "" text -       - %6.2f $1.Umes \
                disprange:-50-+50' colz:dim)


# 1:id&r 2:label
define(`MAGX_COR208_LINE',
       `knob/groupable $1_Iset $2 text nounits - %5.0f $1.Iset \
                alwdrange:-9000-+9000 step:100
        disp           $1_Imes "" text -       - %5.0f $1.Imes \
                normrange:0-5 yelwrange:0-7 \
                c:MAGX_C100($1`'.Iset, $1`'.Imes) colz:important
        disp           $1_Umes "" text -       - %6.2f $1.Umes \
                yelwrange:-1-+24'
                )

# 1:id&r 2:label 3:tip 4:max 5:step 6:c124_base 7/opt:1=positive_only 8/opt:1=1_bit_status
define(`MAGX_COR20_CEAC124_LINE',
       `knob/groupable $1_set $2   text dpyfmt:"%5.2f" r:"$1.Iset" \
                alwdrange:ifelse($7,1,0,-$4)-$4 \
                options:nounits tip:$3
        disp           $1_mes $2_m text dpyfmt:"%5.2f" r:"$1.Imes" \
                normrange:0-1 yelwrange:0-2 disprange:ifelse($7,1,0,-$4)-$4 \
                c:MAGX_C100_OPR($1.Iset, $1.Imes, $1.opr_s1)
##        minmax         $1_dvn ""   text dpyfmt:"%6.3f" r:"$1.Imes"
    container - "" grid nodecor ncols:3 content:3 {
        disp           $1_opr ""   led  shape=circle \
                r:"_all_code; getchan $1.opr_s1; ifelse($8,1,,`getchan $1.opr_s2; mul;') ret"
        RGSWCH(-$1_swc, "", 0, 1, $1.is_on, $1.switch_off, $1.switch_on)
        container -$1_ctl "..." subwin str3:$2": extended controls" \
                content:1 {
            container "" "" grid nodecor 3 content:3 {
                container mps20 "MPS-20" grid noshadow,nocoltitles,norowtitles \
                        base:$1 param3:5 content:eval(5+3) {
                    disp   opr                  "S1"  led shape=circle r:opr_s1
                    disp   opr ifelse($8,1,"-2","S2") led shape=circle r:opr_s2
                    RGSWCH(onoff, "", Off, On, is_on, switch_off, switch_on)
                    disp   mps20_state "M_S" text dpyfmt:"%3.0f" r:mps20_state
                    button rst         "R"   button              r:reset_state
                    container ctl "Control" grid notitle,noshadow,norowtitles \
                            3 content:3 {
                        knob dac    "Set, A"    dpyfmt:"%6.3f" r:Iset      \
                                alwdrange:ifelse($7,1,0,-$4)-$4
                        knob dacspd "MaxSpd, A" dpyfmt:"%6.3f" r:Iset_rate disprange:-5000-+5000
                        disp daccur "Cur, A"    dpyfmt:"%6.3f" r:Iset_cur  disprange:-2500-+2500
                    }
                    noop - - hseparator layinfo:horz=fill
                    container "" "" grid nodecor 6 content:6 {
                        container "mes" "Measurements" grid noshadow,nocoltitles \
                                content:2 {
                            disp   imes    "Imes"    text "" A %9.3f Imes
                            disp   umes    "Umes"    text "" V %9.3f Umes
                        }
                        noop - - vseparator layinfo:vert=fill
                        disp ilk "Ilk" led "shape=circle,color=red"   r:ilk   yelwrange:-0.1-+0.1
                        disp -   ""    led "shape=circle,color=amber" r:c_ilk
                        button rci "R" button                         r:reset_c_ilks
                        knob "" "" choicebs nolabel,compact r:disable \
                                items:"#TEnable\b\blit=green\tDisable\b\blit=red"
                    }
                }
                noop - - vseparator layinfo:vert=fill
                noop
            }
        }
    }
        ')

