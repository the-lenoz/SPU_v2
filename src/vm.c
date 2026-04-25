#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static VmError vm_push(Vm *vm, int32_t value)
{
  if (vm->data_sp >= SPU_DATA_STACK_CAPACITY)
  {
    return VM_ERROR_STACK_OVERFLOW;
  }

  vm->data_stack[vm->data_sp++] = value;
  return VM_OK;
}

static VmError vm_pop(Vm *vm, int32_t *value)
{
  if (vm->data_sp == 0)
  {
    return VM_ERROR_STACK_UNDERFLOW;
  }

  *value = vm->data_stack[--vm->data_sp];
  return VM_OK;
}

static VmError vm_push_call(Vm *vm, uint32_t address)
{
  if (vm->call_sp >= SPU_CALL_STACK_CAPACITY)
  {
    return VM_ERROR_CALL_STACK_OVERFLOW;
  }

  vm->call_stack[vm->call_sp++] = address;
  return VM_OK;
}

static VmError vm_pop_call(Vm *vm, uint32_t *address)
{
  if (vm->call_sp == 0)
  {
    return VM_ERROR_CALL_STACK_UNDERFLOW;
  }

  *address = vm->call_stack[--vm->call_sp];
  return VM_OK;
}

static VmError vm_check_range(const Vm *vm, uint32_t address, uint32_t size)
{
  if (address > vm->ram_size)
  {
    return VM_ERROR_MEMORY_OOB;
  }

  if (size > vm->ram_size - address)
  {
    return VM_ERROR_MEMORY_OOB;
  }

  return VM_OK;
}

static VmError vm_read_u32_le(const Vm *vm, uint32_t address, uint32_t *value)
{
  VmError error = vm_check_range(vm, address, 4u);
  if (error != VM_OK)
  {
    return error;
  }

  *value = ((uint32_t)vm->ram[address]) |
           ((uint32_t)vm->ram[address + 1u] << 8u) |
           ((uint32_t)vm->ram[address + 2u] << 16u) |
           ((uint32_t)vm->ram[address + 3u] << 24u);
  return VM_OK;
}

static VmError vm_write_u32_le(Vm *vm, uint32_t address, uint32_t value)
{
  VmError error = vm_check_range(vm, address, 4u);
  if (error != VM_OK)
  {
    return error;
  }

  vm->ram[address] = (uint8_t)(value & 0xFFu);
  vm->ram[address + 1u] = (uint8_t)((value >> 8u) & 0xFFu);
  vm->ram[address + 2u] = (uint8_t)((value >> 16u) & 0xFFu);
  vm->ram[address + 3u] = (uint8_t)((value >> 24u) & 0xFFu);
  return VM_OK;
}

static VmError vm_jump(Vm *vm, uint32_t target)
{
  if (target >= vm->ram_size)
  {
    return VM_ERROR_PC_OOB;
  }

  vm->pc = target;
  return VM_OK;
}

static VmError vm_poll_events(Vm *vm)
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    if (event.type == SDL_QUIT)
    {
      vm->running = 0;
    }
  }

  return VM_OK;
}

static VmError vm_present(Vm *vm, int force)
{
  uint32_t now = SDL_GetTicks();
  if (!force && (now - vm->last_present_ms) < 16u)
  {
    return VM_OK;
  }

  if (SDL_UpdateTexture(vm->texture,
                        NULL,
                        vm->ram + SPU_VRAM_BASE,
                        (int)SPU_VRAM_PITCH) != 0)
  {
    return VM_ERROR_SDL;
  }

  if (SDL_RenderClear(vm->renderer) != 0)
  {
    return VM_ERROR_SDL;
  }

  if (SDL_RenderCopy(vm->renderer, vm->texture, NULL, NULL) != 0)
  {
    return VM_ERROR_SDL;
  }

  SDL_RenderPresent(vm->renderer);
  vm->last_present_ms = now;
  return VM_OK;
}

static VmError vm_read_immediate(Vm *vm, const InstructionInfo *info, uint32_t *imm32, int32_t *imm8)
{
  if (info->immediate_size == 0u)
  {
    return VM_OK;
  }

  if (info->immediate_size == 1u)
  {
    if (vm->pc >= vm->ram_size)
    {
      return VM_ERROR_PC_OOB;
    }

    *imm8 = (int8_t)vm->ram[vm->pc++];
    return VM_OK;
  }

  if (info->immediate_size == 4u)
  {
    VmError error = vm_read_u32_le(vm, vm->pc, imm32);
    if (error != VM_OK)
    {
      return error;
    }

    vm->pc += 4u;
    return VM_OK;
  }

  return VM_ERROR_INVALID_OPCODE;
}

static VmError vm_step(Vm *vm)
{
  int32_t a = 0;
  int32_t b = 0;
  int32_t cond = 0;
  int32_t addr_i32 = 0;
  uint32_t addr_u32 = 0;
  uint32_t imm32 = 0;
  int32_t imm8 = 0;
  uint32_t tmp_u32 = 0;
  VmError error = VM_OK;

  if (vm->pc >= vm->ram_size)
  {
    return VM_ERROR_PC_OOB;
  }

  uint8_t opcode = vm->ram[vm->pc++];
  const InstructionInfo *info = spu_find_instruction_by_opcode(opcode);
  if (info == NULL)
  {
    return VM_ERROR_INVALID_OPCODE;
  }

  error = vm_read_immediate(vm, info, &imm32, &imm8);
  if (error != VM_OK)
  {
    return error;
  }

  switch (opcode)
  {
    case OP_NOP:
      return VM_OK;

    case OP_HLT:
      vm->running = 0;
      return VM_OK;

    case OP_PUSH8:
      return vm_push(vm, imm8);

    case OP_PUSH32:
      return vm_push(vm, (int32_t)imm32);

    case OP_DUP:
      if (vm->data_sp == 0u)
      {
        return VM_ERROR_STACK_UNDERFLOW;
      }
      return vm_push(vm, vm->data_stack[vm->data_sp - 1u]);

    case OP_DROP:
      return vm_pop(vm, &a);

    case OP_SWAP:
      if (vm->data_sp < 2u)
      {
        return VM_ERROR_STACK_UNDERFLOW;
      }
      a = vm->data_stack[vm->data_sp - 1u];
      b = vm->data_stack[vm->data_sp - 2u];
      vm->data_stack[vm->data_sp - 1u] = b;
      vm->data_stack[vm->data_sp - 2u] = a;
      return VM_OK;

    case OP_OVER:
      if (vm->data_sp < 2u)
      {
        return VM_ERROR_STACK_UNDERFLOW;
      }
      return vm_push(vm, vm->data_stack[vm->data_sp - 2u]);

    case OP_ADD:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a + b);

    case OP_SUB:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a - b);

    case OP_MUL:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a * b);

    case OP_DIV:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      if (b == 0)
      {
        return VM_ERROR_DIVISION_BY_ZERO;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a / b);

    case OP_MOD:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      if (b == 0)
      {
        return VM_ERROR_DIVISION_BY_ZERO;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a % b);

    case OP_NEG:
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, -a);

    case OP_NOT:
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, ~a);

    case OP_AND:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a & b);

    case OP_OR:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a | b);

    case OP_XOR:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a ^ b);

    case OP_EQ:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a == b ? 1 : 0);

    case OP_NE:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a != b ? 1 : 0);

    case OP_LT:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a < b ? 1 : 0);

    case OP_LE:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a <= b ? 1 : 0);

    case OP_GT:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a > b ? 1 : 0);

    case OP_GE:
      error = vm_pop(vm, &b);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, a >= b ? 1 : 0);

    case OP_LOAD8:
      error = vm_pop(vm, &addr_i32);
      if (error != VM_OK)
      {
        return error;
      }
      addr_u32 = (uint32_t)addr_i32;
      error = vm_check_range(vm, addr_u32, 1u);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, (int32_t)vm->ram[addr_u32]);

    case OP_LOAD32:
      error = vm_pop(vm, &addr_i32);
      if (error != VM_OK)
      {
        return error;
      }
      addr_u32 = (uint32_t)addr_i32;
      error = vm_read_u32_le(vm, addr_u32, &tmp_u32);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_push(vm, (int32_t)tmp_u32);

    case OP_STORE8:
      error = vm_pop(vm, &addr_i32);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      addr_u32 = (uint32_t)addr_i32;
      error = vm_check_range(vm, addr_u32, 1u);
      if (error != VM_OK)
      {
        return error;
      }
      vm->ram[addr_u32] = (uint8_t)(a & 0xFF);
      return VM_OK;

    case OP_STORE32:
      error = vm_pop(vm, &addr_i32);
      if (error != VM_OK)
      {
        return error;
      }
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_write_u32_le(vm, (uint32_t)addr_i32, (uint32_t)a);

    case OP_JMP:
      return vm_jump(vm, imm32);

    case OP_JZ:
      error = vm_pop(vm, &cond);
      if (error != VM_OK)
      {
        return error;
      }
      if (cond == 0)
      {
        return vm_jump(vm, imm32);
      }
      return VM_OK;

    case OP_JNZ:
      error = vm_pop(vm, &cond);
      if (error != VM_OK)
      {
        return error;
      }
      if (cond != 0)
      {
        return vm_jump(vm, imm32);
      }
      return VM_OK;

    case OP_CALL:
      error = vm_push_call(vm, vm->pc);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_jump(vm, imm32);

    case OP_RET:
      error = vm_pop_call(vm, &tmp_u32);
      if (error != VM_OK)
      {
        return error;
      }
      return vm_jump(vm, tmp_u32);

    case OP_INP:
      if (scanf("%d", &a) != 1)
      {
        return VM_ERROR_INPUT;
      }
      return vm_push(vm, a);

    case OP_OUT:
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      printf("%d\n", a);
      fflush(stdout);
      return VM_OK;

    case OP_OUTC:
      error = vm_pop(vm, &a);
      if (error != VM_OK)
      {
        return error;
      }
      putchar((unsigned char)(a & 0xFF));
      fflush(stdout);
      return VM_OK;

    default:
      return VM_ERROR_INVALID_OPCODE;
  }
}

VmError vm_init(Vm *vm, uint32_t ram_size)
{
  uint32_t window_width = SPU_VRAM_WIDTH * SPU_WINDOW_SCALE;
  uint32_t window_height = SPU_VRAM_HEIGHT * SPU_WINDOW_SCALE;
  uint32_t vram_end = SPU_VRAM_BASE + SPU_VRAM_SIZE;

  if (vm == NULL)
  {
    return VM_ERROR_BAD_ARGUMENT;
  }

  memset(vm, 0, sizeof(*vm));

  if (ram_size < vram_end)
  {
    return VM_ERROR_BAD_ARGUMENT;
  }

  vm->ram = (uint8_t *)calloc((size_t)ram_size, 1u);
  if (vm->ram == NULL)
  {
    return VM_ERROR_ALLOC;
  }

  vm->ram_size = ram_size;

  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    vm_destroy(vm);
    return VM_ERROR_SDL;
  }

  vm->window = SDL_CreateWindow("SPU v2",
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                (int)window_width,
                                (int)window_height,
                                0);
  if (vm->window == NULL)
  {
    vm_destroy(vm);
    return VM_ERROR_SDL;
  }

  vm->renderer = SDL_CreateRenderer(vm->window, -1, SDL_RENDERER_ACCELERATED);
  if (vm->renderer == NULL)
  {
    vm_destroy(vm);
    return VM_ERROR_SDL;
  }

  vm->texture = SDL_CreateTexture(vm->renderer,
                                  SDL_PIXELFORMAT_XRGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  (int)SPU_VRAM_WIDTH,
                                  (int)SPU_VRAM_HEIGHT);
  if (vm->texture == NULL)
  {
    vm_destroy(vm);
    return VM_ERROR_SDL;
  }

  SDL_RenderSetLogicalSize(vm->renderer, (int)SPU_VRAM_WIDTH, (int)SPU_VRAM_HEIGHT);
  SDL_RenderSetIntegerScale(vm->renderer, SDL_TRUE);

  vm->last_present_ms = SDL_GetTicks();
  return VM_OK;
}

void vm_destroy(Vm *vm)
{
  if (vm == NULL)
  {
    return;
  }

  if (vm->texture != NULL)
  {
    SDL_DestroyTexture(vm->texture);
  }

  if (vm->renderer != NULL)
  {
    SDL_DestroyRenderer(vm->renderer);
  }

  if (vm->window != NULL)
  {
    SDL_DestroyWindow(vm->window);
  }

  SDL_Quit();

  free(vm->ram);
  memset(vm, 0, sizeof(*vm));
}

VmError vm_load_program(Vm *vm, const uint8_t *program, size_t size, uint32_t base_addr)
{
  VmError error = VM_OK;

  if (vm == NULL || program == NULL)
  {
    return VM_ERROR_BAD_ARGUMENT;
  }

  if (size > UINT32_MAX)
  {
    return VM_ERROR_BAD_ARGUMENT;
  }

  error = vm_check_range(vm, base_addr, (uint32_t)size);
  if (error != VM_OK)
  {
    return error;
  }

  memcpy(vm->ram + base_addr, program, size);
  return VM_OK;
}

VmError vm_run(Vm *vm, uint32_t entrypoint)
{
  VmError error = VM_OK;

  if (vm == NULL)
  {
    return VM_ERROR_BAD_ARGUMENT;
  }

  if (entrypoint >= vm->ram_size)
  {
    return VM_ERROR_PC_OOB;
  }

  vm->pc = entrypoint;
  vm->running = 1;

  while (vm->running)
  {
    error = vm_poll_events(vm);
    if (error != VM_OK)
    {
      return error;
    }

    error = vm_step(vm);
    if (error != VM_OK)
    {
      return error;
    }

    error = vm_present(vm, 0);
    if (error != VM_OK)
    {
      return error;
    }
  }

  return vm_present(vm, 1);
}

const char *vm_error_string(VmError error)
{
  switch (error)
  {
    case VM_OK:
      return "ok";
    case VM_ERROR_BAD_ARGUMENT:
      return "bad argument";
    case VM_ERROR_ALLOC:
      return "allocation failed";
    case VM_ERROR_SDL:
      return SDL_GetError();
    case VM_ERROR_STACK_UNDERFLOW:
      return "data stack underflow";
    case VM_ERROR_STACK_OVERFLOW:
      return "data stack overflow";
    case VM_ERROR_CALL_STACK_UNDERFLOW:
      return "call stack underflow";
    case VM_ERROR_CALL_STACK_OVERFLOW:
      return "call stack overflow";
    case VM_ERROR_PC_OOB:
      return "program counter out of bounds";
    case VM_ERROR_MEMORY_OOB:
      return "memory access out of bounds";
    case VM_ERROR_DIVISION_BY_ZERO:
      return "division by zero";
    case VM_ERROR_INVALID_OPCODE:
      return "invalid opcode";
    case VM_ERROR_INPUT:
      return "input error";
    default:
      return "unknown vm error";
  }
}
