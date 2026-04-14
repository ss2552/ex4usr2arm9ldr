rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
LD := arm-none-eabi-ld
OC := arm-none-eabi-objcopy

dir_out := bin

ASFLAGS := -mcpu=mpcore -mlittle-endian
CFLAGS := -Wall -Wextra -MMD -MP -marm -mword-relocations $(ASFLAGS) -fno-builtin -std=c11 -Wno-main -Os -g -flto -fPIC -ffast-math -ffunction-sections -fdata-sections
LDFLAGS := -nostartfiles -Wl,--nmagic,--gc-sections

objects = $(patsubst source/%.s, build/%.o, \
          $(patsubst source/%.c, build/%.o, \
          $(call rwildcard, source, *.s *.c)))

.PHONY: all
all: build/main.bin

.PHONY: arm9
.PHONY: arm11

.PRECIOUS: build/%.bin

build/main.bin: build/main.elf
	@mkdir -p "$(@D)"
	$(OC) -S -O binary $< $@

build/main.elf: $(bundled) $(objects)
	@mkdir -p "$(@D)"
	$(LINK.o) -T linker.ld $(OUTPUT_OPTION) $^

build/arm11.bin: arm11
	@mkdir -p "$(@D)"
	@$(MAKE) -C $<

build/arm9.bin: arm9 build/arm11.bin
	@mkdir -p "$(@D)"
	@$(MAKE) -C $<

build/%.o: source/%.c build/arm9.bin
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

build/%.o: source/%.s build/arm9.bin
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
include $(call rwildcard, build, *.d)
