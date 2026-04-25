#include "spec.h"
#include "vm.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int read_file(const char *path, uint8_t **data, size_t *size)
{
  FILE *file = NULL;
  long file_size = 0;
  uint8_t *buffer = NULL;

  if (path == NULL || data == NULL || size == NULL)
  {
    return -1;
  }

  file = fopen(path, "rb");
  if (file == NULL)
  {
    return -1;
  }

  if (fseek(file, 0, SEEK_END) != 0)
  {
    fclose(file);
    return -1;
  }

  file_size = ftell(file);
  if (file_size < 0)
  {
    fclose(file);
    return -1;
  }

  if (fseek(file, 0, SEEK_SET) != 0)
  {
    fclose(file);
    return -1;
  }

  buffer = (uint8_t *)malloc((size_t)file_size);
  if (buffer == NULL)
  {
    fclose(file);
    return -1;
  }

  if (file_size > 0 && fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size)
  {
    free(buffer);
    fclose(file);
    return -1;
  }

  fclose(file);
  *data = buffer;
  *size = (size_t)file_size;
  return 0;
}

int main(int argc, char **argv)
{
  const char *program_path = NULL;
  uint8_t *program_data = NULL;
  size_t program_size = 0;
  Vm vm = {0};
  VmError error = VM_OK;

  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s program.bin\n", argv[0]);
    return 1;
  }

  program_path = argv[1];
  if (read_file(program_path, &program_data, &program_size) != 0)
  {
    fprintf(stderr, "Failed to read '%s'\n", program_path);
    return 1;
  }

  error = vm_init(&vm, SPU_RAM_SIZE_DEFAULT);
  if (error != VM_OK)
  {
    fprintf(stderr, "vm_init failed: %s\n", vm_error_string(error));
    free(program_data);
    return 1;
  }

  error = vm_load_program(&vm, program_data, program_size, SPU_CODE_BASE);
  free(program_data);
  if (error != VM_OK)
  {
    fprintf(stderr, "vm_load_program failed: %s\n", vm_error_string(error));
    vm_destroy(&vm);
    return 1;
  }

  error = vm_run(&vm, SPU_CODE_BASE);
  if (error != VM_OK)
  {
    fprintf(stderr, "vm_run failed: %s\n", vm_error_string(error));
    vm_destroy(&vm);
    return 1;
  }

  vm_destroy(&vm);
  return 0;
}
