
# 1:device name 2:coeff
define(`MAGX_WALKER_SUBDEV', `
    dev $1_walker iset_walker ~ - $1
    cpoint $1.walker.list         $1_walker.list  $2
    cpoint $1.walker.start        $1_walker.start
    cpoint $1.walker.stop         $1_walker.stop
    cpoint $1.walker.cur_step     $1_walker.cur_step
    cpoint $1.walker.walker_state $1_walker.walker_state
    cpoint $1.walker._devstate    $1_walker._devstate
')


define(`MAGX_OLD_V1000_DEV_LINE', `
    cpoint $4.Iset $1.code$3           r:$5
    cpoint $4.Imes $2.adc`'eval($3*5)  r:$6
')
# 1:g0603_name 2:c0612_name 3-26:{line_name,phys_r} triplets
define(`MAGX_OLD_V1000_DEV', `
    MAGX_OLD_V1000_DEV_LINE($1, $2, 0, $3,  $4,  $5)
    MAGX_OLD_V1000_DEV_LINE($1, $2, 1, $6,  $7,  $8)
    MAGX_OLD_V1000_DEV_LINE($1, $2, 2, $9,  $10, $11)
    MAGX_OLD_V1000_DEV_LINE($1, $2, 3, $12, $13, $14)
    MAGX_OLD_V1000_DEV_LINE($1, $2, 4, $15, $16, $17)
    MAGX_OLD_V1000_DEV_LINE($1, $2, 5, $18, $19, $20)
    MAGX_OLD_V1000_DEV_LINE($1, $2, 6, $21, $22, $23)
    MAGX_OLD_V1000_DEV_LINE($1, $2, 7, $24, $25, $26)
')

define(`MAGX_COR4016_DEV_LINE', `
    cpoint $4.$5.Iset $2.out$3             r:0.00054000 #!!!FLOAT: 5.4/10./1000 # 5.4V<->3A? 1000 - A->mA
    cpoint $4.$5.Iset_rate $2.out_rate$3   r:0.00054000
    cpoint $4.$5.Iset_cur  $2.out_cur$3    r:0.00054000
    cpoint $4.$5.Imes $1.adc`'eval($3*2+0) r:0.00054000 #!!!FLOAT: ^^^
    cpoint $4.$5.Umes $1.adc`'eval($3*2+1) r:0.27027027 #!!!FLOAT: 1/3.7
')
# 1:a40_name 2:c16_name 3:prefix 4-19:line names
define(`MAGX_COR4016_DEV', `
    MAGX_COR4016_DEV_LINE($1, $2,  0, $3, $4)
    MAGX_COR4016_DEV_LINE($1, $2,  1, $3, $5)
    MAGX_COR4016_DEV_LINE($1, $2,  2, $3, $6)
    MAGX_COR4016_DEV_LINE($1, $2,  3, $3, $7)
    MAGX_COR4016_DEV_LINE($1, $2,  4, $3, $8)
    MAGX_COR4016_DEV_LINE($1, $2,  5, $3, $9)
    MAGX_COR4016_DEV_LINE($1, $2,  6, $3, $10)
    MAGX_COR4016_DEV_LINE($1, $2,  7, $3, $11)
    MAGX_COR4016_DEV_LINE($1, $2,  8, $3, $12)
    MAGX_COR4016_DEV_LINE($1, $2,  9, $3, $13)
    MAGX_COR4016_DEV_LINE($1, $2, 10, $3, $14)
    MAGX_COR4016_DEV_LINE($1, $2, 11, $3, $15)
    MAGX_COR4016_DEV_LINE($1, $2, 12, $3, $16)
    MAGX_COR4016_DEV_LINE($1, $2, 13, $3, $17)
    MAGX_COR4016_DEV_LINE($1, $2, 14, $3, $18)
    MAGX_COR4016_DEV_LINE($1, $2, 15, $3, $19)
')

define(`MAGX_COR208_DEV_LINE', `
    cpoint $3.$4.Iset $1.out$2             r:0.00054000 #!!!FLOAT: 5.4/10./1000 # 5.4V<->3A? 1000 - A->mA
    cpoint $3.$4.Iset_rate $1.out_rate$2   r:0.00054000
    cpoint $3.$4.Iset_cur  $1.out_cur$2    r:0.00054000
    cpoint $3.$4.Imes $1.adc`'eval($2*2+0) r:0.00054000 #!!!FLOAT: ^^^
    cpoint $3.$4.Umes $1.adc`'eval($2*2+1) r:0.27027027 #!!!FLOAT: 1/3.7
')
# 1:device_name 2:prefix 3-10:line names
define(`MAGX_COR208_DEV', `
    MAGX_COR208_DEV_LINE($1, 0, $2, $3)
    MAGX_COR208_DEV_LINE($1, 1, $2, $4)
    MAGX_COR208_DEV_LINE($1, 2, $2, $5)
    MAGX_COR208_DEV_LINE($1, 3, $2, $6)
    MAGX_COR208_DEV_LINE($1, 4, $2, $7)
    MAGX_COR208_DEV_LINE($1, 5, $2, $8)
    MAGX_COR208_DEV_LINE($1, 6, $2, $9)
    MAGX_COR208_DEV_LINE($1, 7, $2, $10)
')

define(`MAGX_COR208E_DEV_LINE', `
    cpoint $3.$4.Iset $1.out$2             r:0.00333333 #!!!FLOAT: 10.0/3.0/1000 # 10V<->3A  1000 - A->mA
    cpoint $3.$4.Iset_rate $1.out_rate$2   r:0.00333333
    cpoint $3.$4.Iset_cur  $1.out_cur$2    r:0.00333333
    cpoint $3.$4.Imes $1.adc`'eval($2*2+0) r:0.00333333 #!!!FLOAT: ^^^
    cpoint $3.$4.Umes $1.adc`'eval($2*2+1) r:0.25       #!!!FLOAT: 1/4.0
')
# 1:device_name 2:prefix 3-10:line names
define(`MAGX_COR208E_DEV', `
    MAGX_COR208E_DEV_LINE($1, 0, $2, $3)
    MAGX_COR208E_DEV_LINE($1, 1, $2, $4)
    MAGX_COR208E_DEV_LINE($1, 2, $2, $5)
    MAGX_COR208E_DEV_LINE($1, 3, $2, $6)
    MAGX_COR208E_DEV_LINE($1, 4, $2, $7)
    MAGX_COR208E_DEV_LINE($1, 5, $2, $8)
    MAGX_COR208E_DEV_LINE($1, 6, $2, $9)
    MAGX_COR208E_DEV_LINE($1, 7, $2, $10)
')

# 1:device_name 2:a40_and_d16_base_name 3:N 4:coeff
define(`MAGX_V3H_A40D16_DEV', `
    dev $1 v3h_a40d16    ~ -    $2/$3
    cpoint $1.Iset      $1.Iset      $4
    cpoint $1.Iset_rate $1.Iset_rate $4
    cpoint $1.Iset_cur  $1.Iset_cur  $4
    cpoint $1.dac_mes   $1.dac_mes   $4
    cpoint $1.dcct2     $1.dcct2     $4
    cpoint $1.dcct1     $1.dcct1     $4
    cpoint $1.u1mes     $1.u1mes     $4
    cpoint $1.u2mes     $1.u2mes     $4

    cpoint $1.Imes      $1.dcct1

    cpoint $1.a40d16.out      $2.d16.out$3
    cpoint $1.a40d16.out_rate $2.d16.out_rate$3
    cpoint $1.a40d16.out_cur  $2.d16.out_cur$3
    cpoint $1.a40d16.adc0     $2.a40.adc`'eval($3*5+0)
    cpoint $1.a40d16.adc1     $2.a40.adc`'eval($3*5+1)
    cpoint $1.a40d16.adc2     $2.a40.adc`'eval($3*5+2)
    cpoint $1.a40d16.adc3     $2.a40.adc`'eval($3*5+3)
    cpoint $1.a40d16.adc4     $2.a40.adc`'eval($3*5+4)
    cpoint $1.a40d16.i0       $2.d16.inprb$3
    cpoint $1.a40d16.i1       $2.a40.inprb$3
    cpoint $1.a40d16.o0       $2.d16.outrb$3
    cpoint $1.a40d16.o1       $2.a40.outrb$3

    MAGX_WALKER_SUBDEV($1, $4)
')

# 1:device_name 2:c20_name 3:coeff
define(`MAGX_V1K_CDAC20_DEV', `
    dev $1 v1k_cdac20    ~ -    $2
    cpoint $1.Iset      $1.Iset      $3
    cpoint $1.Iset_rate $1.Iset_rate $3
    cpoint $1.Iset_cur  $1.Iset_cur  $3
    cpoint $1.current1m $1.current1m $3
    cpoint $1.current2m $1.current2m $3
    cpoint $1.adc_dac   $1.adc_dac   $3

    cpoint $1.Imes      $1.current1m

    MAGX_WALKER_SUBDEV($1, $3)
')

# 1:device_name 2:c20_name 3:coeff 4/opt:1=>negated_dcct2
define(`MAGX_IST_CDAC20_DEV', `
    dev $1 ist_cdac20    ~ -    $2
    cpoint $1.Iset      $1.Iset      $3
    cpoint $1.Iset_rate $1.Iset_rate $3
    cpoint $1.Iset_cur  $1.Iset_cur  $3
    cpoint $1.dcct1     $1.dcct1     $3
    cpoint $1.dcct2     $1.dcct2     ifelse($4, 1, -$3, $3)
    cpoint $1.dac_mes   $1.dac_mes   $3
    cpoint $1.adc_dac   $1.adc_dac   $3

    cpoint $1.Imes      $1.dcct1

    MAGX_WALKER_SUBDEV($1, $3)
')

# 1:device_name 2:c124_name
define(`MAGX_MPS20_CEAC124_DEV', `
    dev $1 mps20_ceac124 ~ -    $2
    cpoint $1.Iset      $1.Iset      0.5
    cpoint $1.Iset_rate $1.Iset_rate 0.5
    cpoint $1.Iset_cur  $1.Iset_cur  0.5
    cpoint $1.Imes      $1.Imes      0.5
    cpoint $1.Umes      $1.Umes      0.1
    MAGX_WALKER_SUBDEV($1, 0.5)
')

# 1:device_name 2:c124_name
define(`MAGX_MPS25_CEAC124_DEV', `
    dev $1 mps20_ceac124 ~ -    $2
    cpoint $1.Iset      $1.Iset      0.4
    cpoint $1.Iset_rate $1.Iset_rate 0.4
    cpoint $1.Iset_cur  $1.Iset_cur  0.4
    cpoint $1.Imes      $1.Imes      0.4
    cpoint $1.Umes      $1.Umes      0.1
    MAGX_WALKER_SUBDEV($1, 0.4)
')
