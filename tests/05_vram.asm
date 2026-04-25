; VRAM test: draw a gradient into mapped XRGB8888 framebuffer.
; Close SDL window to stop execution.

start:
  push32 0
  push32 0x00023000
  store32

  push32 0x00800000
  push32 0x00023008
  store32

y_loop:
  push32 0x00023000
  load32
  push32 200
  lt
  jz done

  push32 0
  push32 0x00023004
  store32

  push32 0x00023008
  load32
  push32 0x0002300C
  store32

x_loop:
  push32 0x00023004
  load32
  push32 320
  lt
  jz next_row

  push32 0x00023004
  load32
  push32 65536
  mul

  push32 0x00023000
  load32
  push32 256
  mul
  add

  push32 32
  add

  push32 0x0002300C
  load32
  store32

  push32 0x00023004
  load32
  push32 1
  add
  push32 0x00023004
  store32

  push32 0x0002300C
  load32
  push32 4
  add
  push32 0x0002300C
  store32

  jmp x_loop

next_row:
  push32 0x00023000
  load32
  push32 1
  add
  push32 0x00023000
  store32

  push32 0x00023008
  load32
  push32 1280
  add
  push32 0x00023008
  store32

  jmp y_loop

done:
  jmp done
