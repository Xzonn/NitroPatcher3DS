ROOT := $(abspath ../..)
BUILD := $(ROOT)/build/forwarder
LAUNCHER := $(ROOT)/vendor/TWiLightMenu/slot1launch/arm9/build

include $(DEVKITARM)/ds_rules

CXXFLAGS := -g -Os -mthumb -mthumb-interwork -march=armv5te -mtune=arm946e-s -DARM9 -fno-exceptions -fno-rtti -std=gnu++17
CXXFLAGS += -I$(DEVKITPRO)/libnds/include -I$(ROOT)/vendor/TWiLightMenu/universal/include
CXXFLAGS += -I$(LAUNCHER)
CFLAGS := -g -Os -mthumb -mthumb-interwork -march=armv5te -DARM9 -ffunction-sections -fdata-sections
CFLAGS += -I$(DEVKITPRO)/libnds/include -I$(ROOT)/vendor/TWiLightMenu/universal/include -I$(LAUNCHER)
LDFLAGS := -specs=startup.specs -g -mthumb -mthumb-interwork -Wl,-Map,$(BUILD)/forwarder.map -Wl,--section-start=.twl=0x02400000
OBJECTS := $(BUILD)/main.o $(BUILD)/app_loader.o $(addprefix $(LAUNCHER)/,tonccpy.o my_dldi.o np3d_load.bin.o nitrofs.o)

.PHONY: all
all: $(BUILD)/forwarder.nds

$(BUILD)/main.o: main.cpp
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/app_loader.o: $(ROOT)/source/arm9/app_loader.c $(ROOT)/vendor/TWiLightMenu/universal/source/nds_loader/nds_loader_arm9.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/forwarder.elf: $(OBJECTS) startup.specs $(ROOT)/build/ds_arm9_crt0.o $(ROOT)/vendor/libfat/nds/lib/libfat.a $(ROOT)/vendor/TWiLightMenu/universal/arm9/ds_arm9_twlm.mem
	$(CXX) $(LDFLAGS) $(OBJECTS) -L$(DEVKITPRO)/libnds/lib $(ROOT)/vendor/libfat/nds/lib/libfat.a -lnds9 -o $@

$(BUILD)/forwarder.nds: $(BUILD)/forwarder.elf $(ROOT)/out/NitroPatcherDSi.arm7.elf $(ROOT)/assets/icon.bmp
	ndstool -c $@ -9 $< -7 $(ROOT)/out/NitroPatcherDSi.arm7.elf -b $(ROOT)/assets/icon.bmp "NitroPatcherDS;Game Card Launcher;Xzonn" -g NP3D 01 NITROPATCHER -z 80040000 -u 00030004
