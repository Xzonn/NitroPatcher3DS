.PHONY: all launcher bootstrap verify

export DIAGNOSTIC ?= 0

all: launcher bootstrap

launcher:
	mkdir -p build
	printf '%s\n' "$(DIAGNOSTIC)" > build/launcher-mode.next
	if ! cmp -s build/launcher-mode.next build/launcher-mode; then $(MAKE) -C vendor/TWiLightMenu/slot1launch clean; fi
	mv build/launcher-mode.next build/launcher-mode
	$(MAKE) -C vendor/TWiLightMenu/slot1launch
	mkdir -p out
	cp vendor/TWiLightMenu/slot1launch/slot1launch.nds out/NitroPatcher3DS.nds
	cp vendor/TWiLightMenu/slot1launch/slot1launch.arm9.elf out/NitroPatcher3DS.arm9.elf
	cp vendor/TWiLightMenu/slot1launch/slot1launch.arm7.elf out/NitroPatcher3DS.arm7.elf

bootstrap: build/lzss
	PATH="$(CURDIR)/build:$$PATH" $(MAKE) -C vendor/nds-bootstrap/retail GIT_VER=eb7feabb
	mkdir -p out
	cp vendor/nds-bootstrap/retail/bin/nds-bootstrap.nds out/nds-bootstrap.nds

build/lzss: vendor/nds-bootstrap/lzss.c
	mkdir -p build
	gcc $< -o $@

verify: all
	mkdir -p temp
	ndstool -i out/NitroPatcher3DS.nds
	ndstool -i out/nds-bootstrap.nds
	ndstool -x out/NitroPatcher3DS.nds -9 temp/packed.arm9.bin -7 temp/packed.arm7.bin
	$(DEVKITARM)/bin/arm-none-eabi-objcopy -O binary --remove-section=.twl --remove-section=.twl_bss out/NitroPatcher3DS.arm9.elf temp/linked.arm9.bin
	$(DEVKITARM)/bin/arm-none-eabi-objcopy -O binary --remove-section=.twl --remove-section=.twl_bss out/NitroPatcher3DS.arm7.elf temp/linked.arm7.bin
	cmp temp/packed.arm9.bin temp/linked.arm9.bin
	cmp temp/packed.arm7.bin temp/linked.arm7.bin
