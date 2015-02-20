''*********************************************
''* Bidirectional Packet Driver               *
''*  (C) 2011-2015 David Betz                 *
''* Based on:                                 *
''*  Full-Duplex Serial Driver v1.1 Extended  *
''*   (C) 2006 Parallax, Inc.                 *
''*********************************************

CON

  ' command codes
  #0
  CMD_IDLE      ' must be zero
  CMD_RXPACKET
  CMD_TXPACKET

  ' status codes
  #0
  STATUS_OK
  STATUS_ERROR
  
  ' init offsets
  #0
  INIT_MBOX     ' zeroed by driver after init is done
  INIT_RXPIN
  INIT_TXPIN
  INIT_BAUDRATE
  _INIT_SIZE
  
  ' mailbox offsets
  #0
  MBOX_CMD
  MBOX_TYPE     ' or pointer to type on rx
  MBOX_BUFFER
  MBOX_LENGTH   ' or pointer to length on rx
  MBOX_STATUS
  MBOX_COG      ' not really part of the mailbox
  _MBOX_SIZE
  
  ' character codes
  SOH = $01       ' start of a packet

VAR
  long mbox[_MBOX_SIZE]

PUB start(rxpin, txpin, baudrate) | init[_INIT_SIZE], cogn

'' Start packet driver - starts a cog
'' returns zero on success and non-zero if no cog is available
''

  ' stop the driver if it's already running
  stop

  ' start the driver cog
  init[INIT_MBOX] := @mbox
  init[INIT_RXPIN] := rxpin
  init[INIT_TXPIN] := txpin
  init[INIT_BAUDRATE] := clkfreq / baudrate
  cogn := mbox[MBOX_COG] := cognew(@entry, @init) + 1

  ' if the cog started okay wait for it to finish initializing
  if cogn
    repeat while init[INIT_MBOX] <> 0

  return !cogn

PUB stop
  if mbox[MBOX_COG]
    cogstop(mbox[MBOX_COG]~ - 1)

PUB rx(ptype, buffer, plength)

  mbox[MBOX_BUFFER] := buffer
  mbox[MBOX_LENGTH] := long[plength]
  
  mbox[MBOX_CMD] := CMD_RXPACKET
  repeat while mbox[MBOX_CMD] <> CMD_IDLE

  long[ptype] := mbox[MBOX_TYPE]
  long[plength] := mbox[MBOX_LENGTH]

  return mbox[MBOX_STATUS]

PUB tx(type, buffer, length)

  mbox[MBOX_TYPE] := type
  mbox[MBOX_BUFFER] := buffer
  mbox[MBOX_LENGTH] := length
  
  mbox[MBOX_CMD] := CMD_TXPACKET
  repeat while mbox[MBOX_CMD] <> CMD_IDLE

  return mbox[MBOX_STATUS]

DAT

'***********************************
'* Assembly language serial driver *
'***********************************

                        org
'
'
' Entry
'
entry                   mov     t1, par              'get init structure address

                        rdlong  t2, t1               'get the mailbox address
                        mov     cmd_ptr, t2          'offset 0 - cmd
                        add     t2, #4
                        mov     pkt_type_ptr, t2     'offset 1 - type
                        add     t2, #4
                        mov     pkt_buffer_ptr, t2   'offset 2 - buffer
                        add     t2, #4
                        mov     pkt_length_ptr, t2   'offset 3 - length
                        add     t2, #4
                        mov     pkt_status_ptr, t2   'offset 4 - status

                        add     t1, #4                'get rx_pin
                        rdlong  t2, t1
                        mov     rxmask, #1
                        shl     rxmask, t2

                        add     t1, #4                'get tx_pin
                        rdlong  t2, t1
                        mov     txmask, #1
                        shl     txmask, t2

                        add     t1, #4                'get bit_ticks
                        rdlong  bitticks, t1

                        or      outa, txmask          'initialize the tx pin
                        or      dira, txmask
                        andn    dira, rxmask          'initialize the rx pin

                        rdlong  t1, #0
                        add     t1, cnt
                        waitcnt t1, #0
                        
                        mov     t1, #CMD_IDLE         'no command in progress
                        wrlong  t1, cmd_ptr

                        mov     t1, #0                'signal end of initialization
                        wrlong  t1, par
						
next_cmd                mov     t1, #CMD_IDLE         'no command in progress
                        wrlong  t1, cmd_ptr
:wait                   rdlong  t1, cmd_ptr wz        'wait for a command
              if_z      jmp     #:wait
                        add     t1, #dispatch
                        jmp     t1

dispatch                jmp     #next_cmd             'should never happen
                        jmp     #do_rxpacket
                        jmp     #do_txpacket

' receive a packet
do_rxpacket             rdlong  rcv_ptr, pkt_buffer_ptr
                        rdlong  t1, pkt_length_ptr
                        rdlong  rcv_max, t1
                        call    #rxpacket
              if_c      wrlong  ok_status, pkt_status_ptr
              if_c      wrlong  rcv_type, pkt_type_ptr
              if_c      wrlong  rcv_length, pkt_length_ptr
              if_nc     wrlong  error_status, pkt_status_ptr
                        jmp     #next_cmd
                        
' send a packet
do_txpacket             rdlong  xmt_type, pkt_type_ptr
                        rdlong  xmt_ptr, pkt_buffer_ptr
                        rdlong  xmt_length, pkt_length_ptr
                        call    #txpacket
              if_c      wrlong  ok_status, pkt_status_ptr
              if_nc     wrlong  error_status, pkt_status_ptr
                        jmp     #next_cmd

ok_status               long    STATUS_OK
error_status            long    STATUS_ERROR
                                                
' receive a packet
' input:
'    rcv_ptr points to the receive buffer
'    rcv_max is the maximum number of bytes to receive
' output:
'    rcv_type is the packet type
'    rcv_length is the length of the packet received
'    C is set on return if the packet was received successfully
'    C is clear on return if there was an error
rxpacket                call    #rxbyte
                        cmp     rxdata, #SOH wz
              if_nz     jmp     #rxpacket
                        call    #rxbyte             ' receive packet type
                        mov     rcv_type, rxdata
                        mov     rcv_chk, rxdata
                        call    #rxbyte             ' receive hi byte of packet length
                        mov     rcv_length, rxdata
                        shl     rcv_length, #8
                        add     rcv_chk, rxdata
                        call    #rxbyte             ' receive lo byte of packet length
                        or      rcv_length, rxdata
                        add     rcv_chk, rxdata
                        call    #rxbyte             ' receive header checksum
                        and     rcv_chk, #$ff
                        cmp     rxdata, rcv_chk wz
              if_nz     jmp     #rxerror
                        cmp     rcv_length, rcv_max wz, wc
              if_a      jmp     #rxerror
                        mov     crc, #0
                        mov     rcv_cnt, rcv_length wz
              if_z      jmp     #:crc
:next                   call    #rxbyte             ' receive the next data byte
                        mov     crcdata, rxdata
                        call    #updcrc             ' update the crc
                        wrbyte  rxdata, rcv_ptr
                        add     rcv_ptr, #1
                        djnz    rcv_cnt, #:next
:crc                    call    #rxbyte
                        mov     crcdata, rxdata
                        call    #updcrc             ' update the crc
                        call    #rxbyte
                        mov     crcdata, rxdata
                        call    #updcrc             ' update the crc
                        cmp     crc, #0 wz          ' check the crc
              if_nz     jmp     #rxerror
                        test    $, #1 wc            ' set c to indicate success
rxpacket_ret            ret

rxerror                 test    $, #0 wc            ' clear c to indicate failure
                        jmp     rxpacket_ret

' transmit a packet
' input:
'    xmt_type is the packet type
'    xmt_ptr points to the packet data
'    xmt_length is the number of bytes of data
' output:
'    C is set on return if the packet was received successfully
'    C is clear on return if there was an error
txpacket                mov     txdata, #SOH
                        call    #txbyte
                        mov     txdata, xmt_type
                        mov     xmt_chk, txdata
                        call    #txbyte
                        mov     txdata, xmt_length
                        shr     txdata, #8
                        add     xmt_chk, txdata
                        call    #txbyte
                        mov     txdata, xmt_length
                        and     txdata, #$ff
                        add     xmt_chk, txdata
                        call    #txbyte
                        mov     txdata, xmt_chk
                        and     txdata, #$ff
                        call    #txbyte
                        mov     crc, #0
                        mov     xmt_cnt, xmt_length wz
              if_z      jmp     #:crc
:next                   rdbyte  txdata, xmt_ptr
                        add     xmt_ptr, #1
                        mov     crcdata, txdata
                        call    #updcrc
                        call    #txbyte
                        djnz    xmt_cnt, #:next
:crc                    mov     crcdata, #0
                        call    #updcrc
                        call    #updcrc
                        mov     txdata, crc
                        shr     txdata, #8
                        call    #txbyte
                        mov     txdata, crc
                        and     txdata, #$ff
                        call    #txbyte
                        test    $, #1 wc            ' set c to indicate success
txpacket_ret            ret

' receive a byte
' output:
'    rxdata is the byte received
rxbyte                  waitpne rxmask, rxmask      ' wait for a start bit
                        mov     rxcnt, bitticks     ' wait until half way through the bit time
                        shr     rxcnt, #1
                        add     rxcnt, bitticks
                        add     rxcnt, cnt
                        mov     rxbits, #9          ' receive 8 data bits and the stop bit
:next                   waitcnt rxcnt, bitticks     ' wait until the center of the next bit
                        test    rxmask, ina wc      ' sample the rx pin
                        rcr     rxdata, #1          ' add to the byte being received
                        djnz    rxbits, #:next
                        shr     rxdata, #32-9       ' shift the received data to the low bits
                        and     rxdata, #$ff        ' mask off the stop bit
                        waitpeq rxmask, rxmask      ' wait for the end of the stop bit
rxbyte_ret              ret

' transmit a byte
' input:
'    txdata is the byte to send (destroyed on return)
txbyte                  or      txdata, #$100       ' or in a stop bit
                        shl     txdata, #1          ' shift in a start bit
                        mov     txcnt, cnt
                        add     txcnt, bitticks
                        mov     txbits, #10
:next                   shr     txdata, #1 wc
                        muxc    outa, txmask
                        waitcnt txcnt, bitticks
                        djnz    txbits, #:next
                        waitcnt txcnt, bitticks
txbyte_ret              ret

bitticks                long    0
rxmask                  long    0
rxdata                  long    0
rxbits                  long    0
rxcnt                   long    0
txmask                  long    0
txdata                  long    0
txbits                  long    0
txcnt                   long    0

' update the crc
' input:
'    crc the current crc
'    crcdata the data to add to the crc
' output:
'    crc the updated crc
updcrc                  mov     t1, crc
                        test    t1, #$100 wz
                        shr     t1, #9
                        add     t1, #crctab
                        movs    :load, t1
                        shl     crc, #8
:load                   mov     t1, 0-0
              if_nz     shr     t1, #16
                        xor     crc, t1
                        xor     crc, crcdata
                        and     crc, word_mask
updcrc_ret              ret

crc                     long    0
crcdata                 long    0

'
'
' Initialized data
'
'
zero                    long    0
word_mask               long    $ffff

crctab
    word $0000,  $1021,  $2042,  $3063,  $4084,  $50a5,  $60c6,  $70e7
    word $8108,  $9129,  $a14a,  $b16b,  $c18c,  $d1ad,  $e1ce,  $f1ef
    word $1231,  $0210,  $3273,  $2252,  $52b5,  $4294,  $72f7,  $62d6
    word $9339,  $8318,  $b37b,  $a35a,  $d3bd,  $c39c,  $f3ff,  $e3de
    word $2462,  $3443,  $0420,  $1401,  $64e6,  $74c7,  $44a4,  $5485
    word $a56a,  $b54b,  $8528,  $9509,  $e5ee,  $f5cf,  $c5ac,  $d58d
    word $3653,  $2672,  $1611,  $0630,  $76d7,  $66f6,  $5695,  $46b4
    word $b75b,  $a77a,  $9719,  $8738,  $f7df,  $e7fe,  $d79d,  $c7bc
    word $48c4,  $58e5,  $6886,  $78a7,  $0840,  $1861,  $2802,  $3823
    word $c9cc,  $d9ed,  $e98e,  $f9af,  $8948,  $9969,  $a90a,  $b92b
    word $5af5,  $4ad4,  $7ab7,  $6a96,  $1a71,  $0a50,  $3a33,  $2a12
    word $dbfd,  $cbdc,  $fbbf,  $eb9e,  $9b79,  $8b58,  $bb3b,  $ab1a
    word $6ca6,  $7c87,  $4ce4,  $5cc5,  $2c22,  $3c03,  $0c60,  $1c41
    word $edae,  $fd8f,  $cdec,  $ddcd,  $ad2a,  $bd0b,  $8d68,  $9d49
    word $7e97,  $6eb6,  $5ed5,  $4ef4,  $3e13,  $2e32,  $1e51,  $0e70
    word $ff9f,  $efbe,  $dfdd,  $cffc,  $bf1b,  $af3a,  $9f59,  $8f78
    word $9188,  $81a9,  $b1ca,  $a1eb,  $d10c,  $c12d,  $f14e,  $e16f
    word $1080,  $00a1,  $30c2,  $20e3,  $5004,  $4025,  $7046,  $6067
    word $83b9,  $9398,  $a3fb,  $b3da,  $c33d,  $d31c,  $e37f,  $f35e
    word $02b1,  $1290,  $22f3,  $32d2,  $4235,  $5214,  $6277,  $7256
    word $b5ea,  $a5cb,  $95a8,  $8589,  $f56e,  $e54f,  $d52c,  $c50d
    word $34e2,  $24c3,  $14a0,  $0481,  $7466,  $6447,  $5424,  $4405
    word $a7db,  $b7fa,  $8799,  $97b8,  $e75f,  $f77e,  $c71d,  $d73c
    word $26d3,  $36f2,  $0691,  $16b0,  $6657,  $7676,  $4615,  $5634
    word $d94c,  $c96d,  $f90e,  $e92f,  $99c8,  $89e9,  $b98a,  $a9ab
    word $5844,  $4865,  $7806,  $6827,  $18c0,  $08e1,  $3882,  $28a3
    word $cb7d,  $db5c,  $eb3f,  $fb1e,  $8bf9,  $9bd8,  $abbb,  $bb9a
    word $4a75,  $5a54,  $6a37,  $7a16,  $0af1,  $1ad0,  $2ab3,  $3a92
    word $fd2e,  $ed0f,  $dd6c,  $cd4d,  $bdaa,  $ad8b,  $9de8,  $8dc9
    word $7c26,  $6c07,  $5c64,  $4c45,  $3ca2,  $2c83,  $1ce0,  $0cc1
    word $ef1f,  $ff3e,  $cf5d,  $df7c,  $af9b,  $bfba,  $8fd9,  $9ff8
    word $6e17,  $7e36,  $4e55,  $5e74,  $2e93,  $3eb2,  $0ed1,  $1ef0

'
' Uninitialized data
'
t1                      res     1
t2                      res     1

xmt_type                res     1
xmt_length              res     1
xmt_chk                 res     1
xmt_ptr                 res     1
xmt_cnt                 res     1

rcv_type                res     1
rcv_length              res     1  'packet length
rcv_chk                 res     1  'header checksum
rcv_max                 res     1  'maximum packet data length
rcv_ptr                 res     1  'data buffer pointer
rcv_cnt                 res     1  'data buffer count

cmd_ptr                 res     1
pkt_type_ptr            res     1
pkt_buffer_ptr          res     1
pkt_length_ptr          res     1
pkt_status_ptr          res     1

                        fit     496
