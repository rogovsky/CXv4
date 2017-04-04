# 1:id/r 2:label 3:tip 4:color
define(`KSHD485_CFGBIT_CELL', `
    knob $1 "\n"$2 onoff              "color=$4" tip:$3 r:config_bit_$1
')

# 1:id/r 2:label 3:tip 4:color
define(`KSHD485_STPCND_CELL', `
    knob $1 "\n"$2 onoff              "color=$4" tip:$3 r:stopcond_bit_$1
')

# 1:id/r 2:label 3:tip 4:color
define(`KSHD485_STATUS_CELL', `
    disp $1 "\n"$2 led   "shape=circle,color=$4" tip:$3 r:status_$1
')

# 1:id/r 2:label
define(`KSHD485_BUTTON', `
    button $1 $2 button r:$1
')

grouping main KShD485 "KShD485" \
         lrtb "" content:3 {
    container "info" "Info" grid "nodecor" \
              2 content:2 {
        disp devver "Dev.ver." text "withlabel" dpyfmt:"%3.0f" r:dev_version
        disp devver "serial#"  text "withlabel" dpyfmt:"%3.0f" r:dev_serial
    }

    container "config" "Config" grid "noshadow,nocoltitles" \
            1 0 1 layinfo:newline content:7 {
        KSHD485_BUTTON(save_config, "Save")
        container current "Current" grid nodecor 2 content:2 {
            selector work "Work" selector r:work_current \
                    items:0A\t0.2A\t0.3A\t0.5A\t0.6A\t1.0A\t2.0A\t3.5A
            selector work "Hold" selector r:hold_current \
                    items:0A\t0.2A\t0.3A\t0.5A\t0.6A\t1.0A\t2.0A\t3.5A
        }
        knob hold_dly "Hold dly" text "" "1/30s" "%3.0f" r:hold_delay \
                alwdrange:0-255 layinfo:horz=right
        container cfg_byte "CfgByte" grid "notitle,noshadow,norowtitles" \
                9 content:9 {
            knob config_byte "Byte" text "" "" "%3.0f" r:config_byte
            KSHD485_CFGBIT_CELL(accleave, "a", "Acc.leave",            amber)
            KSHD485_CFGBIT_CELL(leavek,   "k", "Leace Ks",             amber)
            KSHD485_CFGBIT_CELL(softk,    "p", "Soft Ks",              amber)
            KSHD485_CFGBIT_CELL(sensor,   "s", "Sensor=0",             red)
            KSHD485_CFGBIT_CELL(kp,       "+", "K+",                   red)
            KSHD485_CFGBIT_CELL(km,       "-", "K-",                   red)
            KSHD485_CFGBIT_CELL(1,        "1", "Unused",               default)
            KSHD485_CFGBIT_CELL(half,     "8", "Half-speed (8-phase)", blue)
        }
        container stop_cond "StopCond" grid "notitle,noshadow,norowtitles" \
                9 content:9 {
            knob stopcond_byte "Byte" text "" "" "%3.0f" r:stopcond_byte
            KSHD485_STPCND_CELL(7,  "7", "Unused",   default)
            KSHD485_STPCND_CELL(6,  "6", "Unused",   default)
            KSHD485_STPCND_CELL(s1, "S", "Sensor=1", red)
            KSHD485_STPCND_CELL(s0, "s", "Sensor=0", red)
            KSHD485_STPCND_CELL(kp, "+", "K+",       red)
            KSHD485_STPCND_CELL(km, "-", "K-",       red)
            KSHD485_STPCND_CELL(1,  "1", "Unused",   default)
            KSHD485_STPCND_CELL(0,  "0", "Unused",   default)
        }
        container vel "Velocity" grid nodecor 2 content:2 {
            knob min_vel "[" text "withlabel,nounits" "/s" "%5.0f" r:min_velocity \
                    alwdrange:32-65535 layinfo:horz=center
            knob max_vel "-" text "withlabel"        "]/s" "%5.0f" r:max_velocity \
                    alwdrange:32-65535 layinfo:horz=center
        }
        knob accel "Accel" text "" "/s^2" "%5.0f" r:acceleration \
                alwdrange:32-65535 layinfo:horz=center
    }

    container "operate" "Operation" grid "noshadow,nocoltitles" \
            layinfo:newline content:2 {
        container flags "Flags" grid "notitle,noshadow,norowtitles" \
                8 layinfo:horz=right content:8 {
            KSHD485_STATUS_CELL(bc,     "b", "BC",          amber)
            KSHD485_STATUS_CELL(was_k,  "k", "Was K",       amber)
            KSHD485_STATUS_CELL(prec,   "p", "Prec. speed", amber)
            KSHD485_STATUS_CELL(sensor, "s", "Sensor",      red)
            KSHD485_STATUS_CELL(kp,     "+", "K+",          red)
            KSHD485_STATUS_CELL(km,     "-", "K-",          red)
            KSHD485_STATUS_CELL(going,  "g", "Going",       blue)
            KSHD485_STATUS_CELL(ready,  "r", "Ready",       green)
        }
        container steps "Steps" grid "nodecor" \
                3 content:6 {
            knob numsteps "" text dpyfmt:"%6.0f" r:num_steps
            KSHD485_BUTTON(go,          "GO")
            KSHD485_BUTTON(go_wo_accel, "GO unaccel")
            noop "" ""
            KSHD485_BUTTON(stop,        "STOP")
            KSHD485_BUTTON(switchoff,   "Switch OFF")
        }
    }
}

wintitle main "KShD485"
winname  main kshd485
winclass main KShD485
winopts  main ""
