#ifndef SPU_VM_H
#define SPU_VM_H

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdint.h>

#include "spec.h"

typedef enum VmError
{
  VM_OK = 0,
  VM_ERROR_BAD_ARGUMENT,
  VM_ERROR_ALLOC,
  VM_ERROR_SDL,
  VM_ERROR_STACK_UNDERFLOW,
  VM_ERROR_STACK_OVERFLOW,
  VM_ERROR_CALL_STACK_UNDERFLOW,
  VM_ERROR_CALL_STACK_OVERFLOW,
  VM_ERROR_PC_OOB,
  VM_ERROR_MEMORY_OOB,
  VM_ERROR_DIVISION_BY_ZERO,
  VM_ERROR_INVALID_OPCODE,
  VM_ERROR_INPUT,
} VmError;

typedef struct Vm
{
  uint8_t *ram;
  uint32_t ram_size;

  int32_t data_stack[SPU_DATA_STACK_CAPACITY];
  uint32_t data_sp;

  uint32_t call_stack[SPU_CALL_STACK_CAPACITY];
  uint32_t call_sp;

  uint32_t pc;
  int running;

  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  uint32_t last_present_ms;
} Vm;

VmError vm_init(Vm *vm, uint32_t ram_size);
void vm_destroy(Vm *vm);

VmError vm_load_program(Vm *vm, const uint8_t *program, size_t size, uint32_t base_addr);
VmError vm_run(Vm *vm, uint32_t entrypoint);

const char *vm_error_string(VmError error);

#endif
