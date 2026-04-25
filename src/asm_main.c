#include "assembler.h"

#include <stdio.h>

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    fprintf(stderr, "Usage: %s input.asm output.bin\n", argv[0]);
    return 1;
  }

  return assemble_file(argv[1], argv[2]);
}
