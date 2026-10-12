#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
TARGET          := 3dsFM
BUILD           := build
SOURCES         := source/app source/core
INCLUDES        := $(SOURCES)
ROMFS           := romfs

APP_VERSION     := $(strip $(shell cat $(TOPDIR)/VERSION))
APP_TITLE       := 3dsFM
APP_DESCRIPTION := Your music, in stereo, on the 3DS
APP_AUTHOR      := ankushKun
ICON            := resources/icon-3dsfm.png

#---------------------------------------------------------------------------------
ARCH      := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS    := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH)
CFLAGS    += $(INCLUDE) -D__3DS__ -DAPP_VERSION=\"$(APP_VERSION)\" -Wno-psabi
CXXFLAGS  := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS   := -g $(ARCH)
LDFLAGS   := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

# Audio decoders (Opus for streams, mpg123 for local MP3s), HTTPS, and the
# Citro2D/3D renderer. Static archives only pull in what is referenced.
LIBS      := -lcitro2d -lcitro3d -lopusfile -lopus -lvorbisidec -logg -lmpg123 \
             -ljpeg -lpng16 -lz -lctru -lm
LIBDIRS   := $(CTRULIB) $(PORTLIBS)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include -I$(dir)/include/opus) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS := $(OUTPUT).smdh
export APP_ICON := $(TOPDIR)/$(ICON)
export _3DSXFLAGS += --smdh=$(OUTPUT).smdh --romfs=$(CURDIR)/$(ROMFS)

.PHONY: all clean version

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cia

version:
	@echo $(APP_VERSION)

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf: $(OFILES)

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
