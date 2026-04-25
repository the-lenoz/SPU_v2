#ifndef SPU_SPEC_H
#define SPU_SPEC_H

#include <stddef.h>
#include <stdint.h>

enum
{
  SPU_RAM_SIZE_DEFAULT     = 16u * 1024u * 1024u,
  SPU_CODE_BASE            = 0x00001000u,
  SPU_DATA_BASE            = 0x00020000u,

  SPU_DATA_STACK_CAPACITY  = 4096u,
  SPU_CALL_STACK_CAPACITY  = 1024u,

  SPU_VRAM_BASE            = 0x00800000u,
  SPU_VRAM_WIDTH           = 320u,
  SPU_VRAM_HEIGHT          = 200u,
  SPU_VRAM_BYTES_PER_PIXEL = 4u,
  SPU_VRAM_PITCH           = SPU_VRAM_WIDTH * SPU_VRAM_BYTES_PER_PIXEL,
  SPU_VRAM_SIZE            = SPU_VRAM_WIDTH * SPU_VRAM_HEIGHT * SPU_VRAM_BYTES_PER_PIXEL,

  /* SDL window is scaled by this factor on both axes */
  SPU_WINDOW_SCALE         = 4u,
};

typedef enum SpuOpcode
{
  OP_NOP     = 0x00,
  OP_HLT     = 0x01,

  OP_PUSH8   = 0x02, /* imm8  */
  OP_PUSH32  = 0x03, /* imm32 */

  OP_DUP     = 0x04,
  OP_DROP    = 0x05,
  OP_SWAP    = 0x06,
  OP_OVER    = 0x07,

  OP_ADD     = 0x08,
  OP_SUB     = 0x09,
  OP_MUL     = 0x0A,
  OP_DIV     = 0x0B,
  OP_MOD     = 0x0C,
  OP_NEG     = 0x0D,
  OP_NOT     = 0x0E,
  OP_AND     = 0x0F,
  OP_OR      = 0x10,
  OP_XOR     = 0x11,

  OP_EQ      = 0x12,
  OP_NE      = 0x13,
  OP_LT      = 0x14,
  OP_LE      = 0x15,
  OP_GT      = 0x16,
  OP_GE      = 0x17,

  OP_LOAD8   = 0x18,
  OP_LOAD32  = 0x19,
  OP_STORE8  = 0x1A,
  OP_STORE32 = 0x1B,

  OP_JMP     = 0x1C, /* imm32 absolute address */
  OP_JZ      = 0x1D, /* imm32 absolute address */
  OP_JNZ     = 0x1E, /* imm32 absolute address */
  OP_CALL    = 0x1F, /* imm32 absolute address */
  OP_RET     = 0x20,

  OP_INP     = 0x21,
  OP_OUT     = 0x22,
  OP_OUTC    = 0x23,
} SpuOpcode;

typedef struct InstructionInfo
{
  const char *name;
  uint8_t opcode;
  uint8_t immediate_size;
  const char *stack_effect;
  const char *description;
} InstructionInfo;

#define SPU_INSTRUCTION_LIST(X)                                                                                 \
  X("nop",     OP_NOP,     0, "( -- )",                   "No operation")                                    \
  X("hlt",     OP_HLT,     0, "( -- )",                   "Stop VM execution")                              \
  X("push8",   OP_PUSH8,   1, "( -- imm8 )",              "Push sign-extended 8-bit constant")             \
  X("push32",  OP_PUSH32,  4, "( -- imm32 )",             "Push 32-bit constant bits")                      \
  X("dup",     OP_DUP,     0, "( a -- a a )",             "Duplicate top element")                          \
  X("drop",    OP_DROP,    0, "( a -- )",                 "Drop top element")                               \
  X("swap",    OP_SWAP,    0, "( a b -- b a )",           "Swap top two elements")                          \
  X("over",    OP_OVER,    0, "( a b -- a b a )",         "Copy second element to top")                     \
  X("add",     OP_ADD,     0, "( a b -- a+b )",           "Signed 32-bit addition")                         \
  X("sub",     OP_SUB,     0, "( a b -- a-b )",           "Signed 32-bit subtraction")                      \
  X("mul",     OP_MUL,     0, "( a b -- a*b )",           "Signed 32-bit multiplication")                   \
  X("div",     OP_DIV,     0, "( a b -- a/b )",           "Signed division, b must be non-zero")            \
  X("mod",     OP_MOD,     0, "( a b -- a%b )",           "Signed remainder, b must be non-zero")           \
  X("neg",     OP_NEG,     0, "( a -- -a )",              "Arithmetic negation")                            \
  X("not",     OP_NOT,     0, "( a -- ~a )",              "Bitwise NOT")                                    \
  X("and",     OP_AND,     0, "( a b -- a&b )",           "Bitwise AND")                                    \
  X("or",      OP_OR,      0, "( a b -- a|b )",           "Bitwise OR")                                     \
  X("xor",     OP_XOR,     0, "( a b -- a^b )",           "Bitwise XOR")                                    \
  X("eq",      OP_EQ,      0, "( a b -- a==b )",          "Push 1 if equal, else 0")                        \
  X("ne",      OP_NE,      0, "( a b -- a!=b )",          "Push 1 if not equal, else 0")                    \
  X("lt",      OP_LT,      0, "( a b -- a<b )",           "Push 1 if less, else 0")                         \
  X("le",      OP_LE,      0, "( a b -- a<=b )",          "Push 1 if less-or-equal, else 0")                \
  X("gt",      OP_GT,      0, "( a b -- a>b )",           "Push 1 if greater, else 0")                      \
  X("ge",      OP_GE,      0, "( a b -- a>=b )",          "Push 1 if greater-or-equal, else 0")             \
  X("load8",   OP_LOAD8,   0, "( addr -- value )",        "Read unsigned 8-bit value from RAM")             \
  X("load32",  OP_LOAD32,  0, "( addr -- value )",        "Read 32-bit little-endian value from RAM")       \
  X("store8",  OP_STORE8,  0, "( value addr -- )",        "Write low 8 bits of value to RAM")               \
  X("store32", OP_STORE32, 0, "( value addr -- )",        "Write 32-bit little-endian value to RAM")        \
  X("jmp",     OP_JMP,     4, "( -- )",                   "Jump to absolute address")                        \
  X("jz",      OP_JZ,      4, "( cond -- )",              "Jump if cond == 0")                              \
  X("jnz",     OP_JNZ,     4, "( cond -- )",              "Jump if cond != 0")                              \
  X("call",    OP_CALL,    4, "( -- )",                   "Push return address and jump")                    \
  X("ret",     OP_RET,     0, "( -- )",                   "Return to saved call address")                    \
  X("inp",     OP_INP,     0, "( -- value )",             "Read int32 from stdin and push")                  \
  X("out",     OP_OUT,     0, "( value -- )",             "Pop int32 and print as decimal")                  \
  X("outc",    OP_OUTC,    0, "( value -- )",             "Pop int32 and print low byte as char")

static const InstructionInfo kInstructionTable[] = {
#define SPU_MAKE_ROW(name_, opcode_, imm_size_, stack_effect_, description_) \
  { name_, opcode_, imm_size_, stack_effect_, description_ },
  SPU_INSTRUCTION_LIST(SPU_MAKE_ROW)
#undef SPU_MAKE_ROW
};

static inline size_t spu_instruction_count(void)
{
  return sizeof(kInstructionTable) / sizeof(kInstructionTable[0]);
}

static inline const InstructionInfo *spu_find_instruction_by_opcode(uint8_t opcode)
{
  size_t count = spu_instruction_count();
  for (size_t i = 0; i < count; ++i)
  {
    if (kInstructionTable[i].opcode == opcode)
    {
      return &kInstructionTable[i];
    }
  }
  return NULL;
}

/*
 * Video memory format.
 * Mapped region [SPU_VRAM_BASE, SPU_VRAM_BASE + SPU_VRAM_SIZE).
 * Pixel format for each 32-bit cell is 0x00RRGGBB.
 * VM shows this region through SDL texture.
 */

#endif
