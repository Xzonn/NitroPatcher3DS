.PHONY: all launcher bootstrap verify patch-arm9 host

export DIAGNOSTIC ?= 0
export LAUNCH_TRACE ?= 0
export PREPARE_TRACE ?= 0
export NP3D_VERSION := $(shell git describe --always --dirty)

all: launcher bootstrap

patch-arm9:
	$(MAKE) -C source/patch MODE=arm9

host:
	$(MAKE) -C source/patch MODE=host

launcher: patch-arm9 bootstrap
	mkdir -p build
	printf '%s %s\n' "$(DIAGNOSTIC)" "$(LAUNCH_TRACE)" > build/launcher-mode.next
	if ! cmp -s build/launcher-mode.next build/launcher-mode; then $(MAKE) -C vendor/TWiLightMenu/slot1launch clean; fi
	mv build/launcher-mode.next build/launcher-mode
	$(MAKE) -C vendor/TWiLightMenu/universal/bootloader_app LOADBIN=$(CURDIR)/build/np3d_load.bin
	mkdir -p vendor/TWiLightMenu/slot1launch/data
	cp build/np3d_load.bin vendor/TWiLightMenu/slot1launch/data/np3d_load.bin
	rm -f vendor/TWiLightMenu/slot1launch/data/np3d_bootstrap.twl.bin
	python3 source/build_resources.py
	$(MAKE) -C vendor/TWiLightMenu/slot1launch
	mkdir -p out
	cp vendor/TWiLightMenu/slot1launch/slot1launch.nds out/NitroPatcher3DS.nds
	cp vendor/TWiLightMenu/slot1launch/slot1launch.arm9.elf out/NitroPatcher3DS.arm9.elf
	cp vendor/TWiLightMenu/slot1launch/slot1launch.arm7.elf out/NitroPatcher3DS.arm7.elf
	mkdir -p out/licenses
	cp LICENSE out/licenses/NitroPatcher3DS.txt
	cp vendor/md4c/LICENSE.md out/licenses/md4c.txt
	cp vendor/fmt/LICENSE out/licenses/fmt.txt
	cp assets/fonts/Gulim-OFL.txt assets/fonts/Gulim-AUTHORS.txt out/licenses/
	cp /opt/np3d/liblzma/COPYING out/licenses/liblzma.txt

bootstrap: build/lzss
	printf '%s\n' "$(LAUNCH_TRACE)" > build/bootstrap-mode.next
	if ! cmp -s build/bootstrap-mode.next build/bootstrap-mode; then for directory in arm9 bootloaderi bootloaderi2 cardenginei/arm7 cardenginei/arm7_alt cardenginei/arm7_twlsdk cardenginei/arm7_twlsdk3; do $(MAKE) -C vendor/nds-bootstrap/retail/$$directory clean || exit $$?; done; fi
	mv build/bootstrap-mode.next build/bootstrap-mode
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
