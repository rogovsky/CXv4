#!/bin/sh

#
# Note: this file REQUIRES bash or zsh, debian's dash wouldn't suffice
#       because dash doesn't support $'strings'
#

if [ -z "$1" ]
then
    echo "$0: usage:" >&2
    echo "    Create .c: $0 -Mc builtins.h-name" >&2
    echo "    Create .h: $0 -Mh" >&2
    echo "    Create mk: $0 -Mm VARNAME" >&2
    exit 1
fi

case "$1" in
    '-Mc')
        ;;
    '-Mm')
        if [ -z "$2" ]
        then
            echo "$0: -Mm requires a VARNAME parameter" >&2
            exit 1
        fi
        ;;
    *)
        echo "$0: unknown mode \"$1\"" >&2
        exit 1
esac

DECLARES_LIST=""
BUILTINS_LIST=""
WAS_cxsd_frontend=""
WAS_cxsd_dbldr=""
WAS_cxsd_drvlyr=""
WAS_cxsd_extlib=""
WAS_dat_plugin=""
WAS_motif_knobset=""
while read MOD_TYPE MOD_NAME
do
    # Ignore blank lines
    [ -z "$MOD_TYPE" ] && continue

    case "$MOD_TYPE" in
        '#'*)
            #echo '/*comment*/'
            ;;
        'cxsd_frontend')
            DECLARES_LIST="$DECLARES_LIST""extern DECLARE_CXSD_FRONTEND ($MOD_NAME);"$'\n'
            BUILTINS_LIST="$BUILTINS_LIST""    &(CXSD_FRONTEND_MODREC_NAME($MOD_NAME).mr),"$'\n'
            OBJECTS_LIST="$OBJECTS_LIST"$' \\\n'"    cxsd_fe_${MOD_NAME}.o"
            WAS_cxsd_frontend=Y
            ;;
        'cxsd_dbldr')
            DECLARES_LIST="$DECLARES_LIST""extern DECLARE_CXSD_DBLDR    ($MOD_NAME);"$'\n'
            BUILTINS_LIST="$BUILTINS_LIST""    &(CXSD_DBLDR_MODREC_NAME   ($MOD_NAME).mr),"$'\n'
            OBJECTS_LIST="$OBJECTS_LIST"$' \\\n'"    cxsd_db_${MOD_NAME}_ldr.o"
            WAS_cxsd_dbldr=Y
            ;;
        'cxsd_driver')
            DECLARES_LIST="$DECLARES_LIST""extern DECLARE_CXSD_DRIVER   ($MOD_NAME);"$'\n'
            BUILTINS_LIST="$BUILTINS_LIST""    &(CXSD_DRIVER_MODREC_NAME  ($MOD_NAME).mr),"$'\n'
            OBJECTS_LIST="$OBJECTS_LIST"$' \\\n'"    ${MOD_NAME}_drv.o"
            WAS_cxsd_drvlyr=Y
            ;;
        'cxsd_layer')
            DECLARES_LIST="$DECLARES_LIST""extern DECLARE_CXSD_LAYER    ($MOD_NAME);"$'\n'
            BUILTINS_LIST="$BUILTINS_LIST""    &(CXSD_LAYER_MODREC_NAME   ($MOD_NAME).mr),"$'\n'
            OBJECTS_LIST="$OBJECTS_LIST"$' \\\n'"    ${MOD_NAME}_lyr.o"
            WAS_cxsd_drvlyr=Y
            ;;
        'cxsd_ext')
            DECLARES_LIST="$DECLARES_LIST""extern DECLARE_CXSD_EXT      ($MOD_NAME);"$'\n'
            BUILTINS_LIST="$BUILTINS_LIST""    &(CXSD_EXT_MODREC_NAME     ($MOD_NAME).mr),"$'\n'
            OBJECTS_LIST="$OBJECTS_LIST"$' \\\n'"    ${MOD_NAME}_ext.o"
            WAS_cxsd_extlib=Y
            ;;
        'cxsd_lib')
            DECLARES_LIST="$DECLARES_LIST""extern DECLARE_CXSD_LIB      ($MOD_NAME);"$'\n'
            BUILTINS_LIST="$BUILTINS_LIST""    &(CXSD_LIB_MODREC_NAME     ($MOD_NAME).mr),"$'\n'
            OBJECTS_LIST="$OBJECTS_LIST"$' \\\n'"    ${MOD_NAME}_lib.o"
            WAS_cxsd_extlib=Y
            ;;
        'dat_plugin')
            DECLARES_LIST="$DECLARES_LIST""extern CDA_DECLARE_DAT_PLUGIN($MOD_NAME);"$'\n'
            BUILTINS_LIST="$BUILTINS_LIST""    &(CDA_DAT_P_MODREC_NAME    ($MOD_NAME).mr),"$'\n'
            OBJECTS_LIST="$OBJECTS_LIST"$' \\\n'"    cda_d_${MOD_NAME}.o"
            WAS_dat_plugin=Y
            ;;
        'motif_knobset')
            ;;
        *)
            echo "$0: unknown mod_type \"$MOD_TYPE\"" >&2
            exit 1
    esac
done

######################################################################

case "$1" in

'-Mc')

echo "#include <stdio.h>"
echo ""
echo "#include \"misclib.h\""
echo "#include \"main_builtins.h\""
echo ""
[ -n "$WAS_cxsd_frontend" ]  &&  echo "#include \"cxsd_frontend.h\""
[ -n "$WAS_cxsd_dbldr"    ]  &&  echo "#include \"cxsd_dbP.h\""
[ -n "$WAS_cxsd_drvlyr"   ]  &&  echo "#include \"cxsd_driver.h\""
[ -n "$WAS_cxsd_extlib"   ]  &&  echo "#include \"cxsd_extension.h\""
[ -n "$WAS_dat_plugin"    ]  &&  echo "#include \"cdaP.h\""

echo ""
echo "//////////////////////////////////////////////////////////////////////"
echo ""
echo "$DECLARES_LIST"
echo ""
echo "static cx_module_rec_t *builtins_list[] ="
echo "{"
echo "$BUILTINS_LIST"
echo "};"

cat <<'__END__'
//////////////////////////////////////////////////////////////////////

int  InitBuiltins(init_builtins_err_notifier_t err_notifier)
{
  int  n;

    for (n = 0;  n < countof(builtins_list);  n++)
    {
        if (builtins_list[n]->init_mod != NULL  &&
            builtins_list[n]->init_mod() < 0)
        {
            if (err_notifier != NULL)
            {
                if (err_notifier(builtins_list[n]->magicnumber,
                                 builtins_list[n]->name) != 0)
                    return -1;
            }
            else
                fprintf(stderr, "%s %s(): %08x.%s.init_mod() failed\n",
                        strcurtime(), __FUNCTION__,
                        builtins_list[n]->magicnumber,
                        builtins_list[n]->name);
        }
    }

    return 0;
}

void TermBuiltins(void)
{
  int  n;

    for (n = countof(builtins_list) - 1;  n >= 0;  n--)
        if (builtins_list[n]->term_mod != NULL)
            builtins_list[n]->term_mod();
}
__END__

;;

'-Mm')

echo "$2=""$OBJECTS_LIST"

;;

esac
