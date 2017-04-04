
# 1:hwdev_name 2:cpoint_base 3:hwdev_line
define(`U0632_DEV_LINE', `
    cpoint $2.data        $1.data$3
    cpoint $2.bias_r0     $1.bias_r0_$3
    cpoint $2.bias_r1     $1.bias_r1_$3
    cpoint $2.bias_r2     $1.bias_r2_$3
    cpoint $2.bias_r3     $1.bias_r3_$3

    cpoint $2.online      $1.online$3
    cpoint $2.cur_M       $1.cur_M$3
    cpoint $2.cur_P       $1.cur_P$3
    cpoint $2.M           $1.M$3
    cpoint $2.P           $1.P$3
    cpoint $2.cur_numpts  $1.const32
')

# 1:u0632_name 2-31:lina_names
define(`U0632_DEV', `
    U0632_DEV_LINE($1, $2,  0)
    U0632_DEV_LINE($1, $3,  1)
    U0632_DEV_LINE($1, $4,  2)
    U0632_DEV_LINE($1, $5,  3)
    U0632_DEV_LINE($1, $6,  4)
    U0632_DEV_LINE($1, $7,  5)
    U0632_DEV_LINE($1, $8,  6)
    U0632_DEV_LINE($1, $9,  7)
    U0632_DEV_LINE($1, $10, 8)
    U0632_DEV_LINE($1, $11, 9)
    U0632_DEV_LINE($1, $12, 10)
    U0632_DEV_LINE($1, $13, 11)
    U0632_DEV_LINE($1, $14, 12)
    U0632_DEV_LINE($1, $15, 13)
    U0632_DEV_LINE($1, $16, 14)
    U0632_DEV_LINE($1, $17, 15)
    U0632_DEV_LINE($1, $18, 16)
    U0632_DEV_LINE($1, $19, 17)
    U0632_DEV_LINE($1, $20, 18)
    U0632_DEV_LINE($1, $21, 19)
    U0632_DEV_LINE($1, $22, 20)
    U0632_DEV_LINE($1, $23, 21)
    U0632_DEV_LINE($1, $24, 22)
    U0632_DEV_LINE($1, $25, 23)
    U0632_DEV_LINE($1, $26, 24)
    U0632_DEV_LINE($1, $27, 25)
    U0632_DEV_LINE($1, $28, 26)
    U0632_DEV_LINE($1, $29, 27)
    U0632_DEV_LINE($1, $30, 28)
    U0632_DEV_LINE($1, $31, 29)
')
