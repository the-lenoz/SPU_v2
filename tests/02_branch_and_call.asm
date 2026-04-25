; Sum from 1 to 10 using loop + call/ret.
; Expected output: 55

start:
  push32 1
  push32 0x00021000
  store32

  push32 0
  push32 0x00021004
  store32

loop:
  push32 0x00021000
  load32
  push32 10
  le
  jz done

  call add_i_to_sum

  push32 0x00021000
  load32
  push32 1
  add
  push32 0x00021000
  store32

  jmp loop

done:
  push32 0x00021004
  load32
  out
  hlt

add_i_to_sum:
  push32 0x00021004
  load32
  push32 0x00021000
  load32
  add
  push32 0x00021004
  store32
  ret
