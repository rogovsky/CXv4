     define(`forloop',
            `pushdef(`$1', `$2')_forloop(`$1', `$2', `$3', `$4')popdef(`$1')')
     define(`_forloop',
            `$4`'ifelse($1, `$3', ,
                   `define(`$1', incr($1))_forloop(`$1', `$2', `$3', `$4')')')
     define(`fordown',
            `pushdef(`$1', `$2')_fordown(`$1', `$2', `$3', `$4')popdef(`$1')')
     define(`_fordown',
            `$4`'ifelse($1, `$3', ,
                   `define(`$1', decr($1))_fordown(`$1', `$2', `$3', `$4')')')
#---------------------------------------------------------------------

# GID25_MAIN_LINE(id_prefix, r_prefix, 1st_knob_label)
# Note: r_prefix, if specified, MUST contain a trailing '.' separator
define(`GID25_MAIN_LINE', `
    knob $1Uset     "$3" text  "" "" %7.2f   r:$2Uset
    disp $1Uuvh     ""   text  "" "" %7.2f   r:$2Uuvh
    disp $1Imes     ""   text  "" "" %7.2f   r:$2Imes
    knob $1onoff    "" choicebs              r:$2onoff \
            items:"#TOff\b\blit=red\tOn\b\blit=green"
    disp $1err   "Error" led   "color=red"   r:$2err
    knob $1ena_gid  ""   onoff "color=green" r:$2ena_gid
    knob $1Dly_gid  ""   text  "" "" %6.1f   r:$2Dly_gid
    knob $1ena_vsdc ""   onoff "color=green" r:$2ena_vsdc
    knob $1Dly_vsdc ""   text  "" "" %6.1f   r:$2Dly_vsdc
')

# GID25_K500_LINE(id_prefix, r_prefix, 1st_knob_label)
# Note: r_prefix, if specified, MUST contain a trailing '.' separator
define(`GID25_K500_LINE', `
    knob $1Uset     "$3" text  "" "" %7.2f   r:$2Uset
    disp $1Umes     ""   text  "" "" %7.2f   r:$2Umes_raw
    disp $1Uuvh     ""   text  "" "" %7.2f   r:$2Uuvh_raw
    disp $1Imes     ""   text  "" "" %7.2f   r:$2Imes
    knob $1onoff    "" choicebs              r:$2onoff \
            items:"#TOff\b\blit=red\tOn\b\blit=green"
    disp $1err   "Error" led   "color=red"   r:$2err
    knob $1ena_gid  ""   onoff "color=green" r:$2ena_gid
    knob $1Dly_gid  ""   text  "" "" %6.1f   r:$2Dly_gid
    knob $1ena_vsdc ""   onoff "color=green" r:$2ena_vsdc
    knob $1Dly_vsdc ""   text  "" "" %6.1f   r:$2Dly_vsdc
')

# Old: #    disp $1Umes     ""   text  "" "" %7.2f   r:$2Umes

define(`GID25_CEAC124_CHAN_ADC_n_COUNT', 16)
define(`GID25_CEAC124_CHAN_OUT_n_COUNT', 4)
define(`GID25_CEAC124_DAC_LINE', `
    knob dac$1_set dpyfmt:"%7.4f" r:out$1      alwdrange:-10.0-+10.0
    knob dac$1_spd dpyfmt:"%7.4f" r:out_rate$1 alwdrange:-20.0-+20.0
    disp dac$1_cur dpyfmt:"%7.4f" r:out_cur$1
')
define(`GID25_CEAC124_ADC_LINE', `
    disp adc$1 dpyfmt:"%9.6f" r:adc$1
')
define(`GID25_CAN_IO_LINE', `
    disp inprb$1 "" led   "shape=circle" r:inprb$1
    knob outrb$1 "" onoff "color=blue"   r:outrb$1
')
# 1:id 2:button_label 3:window_title 4:base_ref
define(`GID25_CEAC124_BUTTON', `
    container/unsavable $1 $2 subwin str3:$3 base:$4 content:1 {
        container "" "" grid nodecor 3 content:3 {

            container "" "DAC" grid "" \
                    3 0 3 \
                    "Set, V\tMaxSpd, V/s\tCur, V" \
                    "forloop(`N', 0, eval(GID25_CEAC124_CHAN_OUT_n_COUNT-1), `N\t')" \
                    content:eval(GID25_CEAC124_CHAN_OUT_n_COUNT*3+3) {
                disp   hw_ver HW    text   "withlabel" dpyfmt:"%-2.0f" r:hw_ver
                disp   sw_ver SW    text   "withlabel" dpyfmt:"%-2.0f" r:sw_ver
                button reset  Reset button                             r:do_reset
                forloop(`N', 0, eval(GID25_CEAC124_CHAN_OUT_n_COUNT-1), `GID25_CEAC124_DAC_LINE(N)')
            }

            container "" "ADC" grid "" \
                    1 4 4 \
                    "U, V" \
                    "forloop(`N', 0, eval(GID25_CEAC124_CHAN_ADC_n_COUNT-5), `N\t')T\tPWR\t10V\t0V" \
                    content:eval(GID25_CEAC124_CHAN_ADC_n_COUNT+4) {
                selector adc_time "" selector r:adc_timecode \
                         items:"1ms\t2ms\t5ms\t10ms\t20ms\t40ms\t80ms\t160ms"
                selector adc_gain "" selector r:adc_gain \
                         items:"x1\tx10\t\nx100\t\nx1000"
                knob     adc_beg "Ch" text "withlabel" dpyfmt:"%2.0f" r:adc_beg
                knob     adc_end "-"  text "withlabel" dpyfmt:"%2.0f" r:adc_end
                forloop(`N', 0, eval(GID25_CEAC124_CHAN_ADC_n_COUNT-1), `GID25_CEAC124_ADC_LINE(N)')
            }

            container "" "I/O" grid "" \
                    2 0 0 "I\tO" "0\t1\t2\t3" \
                    content:8 {
                forloop(`N', 0, 3, `GID25_CAN_IO_LINE(N)')
            }

        }
    }
')

define(`GID25_CGVI8_LINE', `
    knob is_on$1 ""  onoff "color=green"             r:mask_bit_$1
    knob raw$1   ""  text  ""            "q"  %5.0f  raw$1   alwdrange:0-65535
    knob hns$1   "=" text  "withlabel"   "us" %11.1f quant$1 alwdrange:0-214700000
')
# 1:id 2:button_label 3:window_title 4:base_ref
define(`GID25_CGVI8_BUTTON', `
    container/unsavable $1 $2 subwin str3:$3 base:$4 content:1 {
        container "" "CGVI8" grid "" \
                3 0 4 \
                " \t16-bit code\tTime" \
                "forloop(`N', 0, 7, `N\t')" \
                content:eval(3*8+4) {
                    disp   hw_ver HW    text   "withlabel" dpyfmt:"%-2.0f" r:hw_ver
                    disp   sw_ver SW    text   "withlabel" dpyfmt:"%-2.0f" r:sw_ver
                    selector prescaler "q=" selector "align=right" r:prescaler \
                            items:"#T100 ns\t200 ns\t400 ns\t800 ns\t1.60 us\t3.20 us\t6.40 us\t12.8 us\t25.6 us\t51.2 us\t102 us\t205 us\t410 us\t819 us\t1.64 ms\t3.28 ms"
                    button progstart ProgStart arrowrt                     r:progstart
                    forloop(`N', 0, 7, `GID25_CGVI8_LINE(N)')
        }
    }
')
