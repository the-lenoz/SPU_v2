; Basic stack arithmetic and stack-manipulation test.

start:
  push8 7
  push8 5
  add
  dup
  out

  push8 3
  mul
  push8 4
  sub
  out

  hlt
