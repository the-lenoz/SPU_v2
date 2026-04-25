; load/store memory test.
; Expected output:
; 65
; 16961

start:
  push32 0
  push32 0x00022000
  store32

  push32 65
  push32 0x00022000
  store8

  push32 0x00022000
  load8
  out

  push32 66
  push32 0x00022001
  store8

  push32 0x00022000
  load32
  out

  hlt
