CFLAGS += -Wall -Wextra -Wpedantic -O3 -g

SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)

SRC_DIR := src
BUILD_DIR := build

VM_SOURCES := $(SRC_DIR)/vm_main.c $(SRC_DIR)/vm.c
ASM_SOURCES := $(SRC_DIR)/asm_main.c $(SRC_DIR)/assembler.c

VM_OBJECTS := $(addprefix $(BUILD_DIR)/, $(notdir $(VM_SOURCES:.c=.o)))
ASM_OBJECTS := $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES:.c=.o)))

.PHONY: all clean test-programs

all: vm asm

vm: $(VM_OBJECTS)
	$(CC) $(CFLAGS) $(VM_OBJECTS) $(SDL_LIBS) -o vm

asm: $(ASM_OBJECTS)
	$(CC) $(CFLAGS) $(ASM_OBJECTS) -o asm

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -I$(SRC_DIR) -c $< -o $@

test-programs: asm
	mkdir -p tests/bin
	for source in tests/*.asm; do ./asm "$$source" "tests/bin/$$(basename "$$source" .asm).bin"; done

clean:
	rm -rf $(BUILD_DIR) vm asm tests/bin
