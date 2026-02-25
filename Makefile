# Xbox Homebrew Project Makefile
# Built with nxdk - https://github.com/XboxDev/nxdk

# Project name (override with: make XBE_TITLE=MyGame)
XBE_TITLE = vortexion

# nxdk path - relative to this project dir
NXDK_DIR ?= ../../toolchain/nxdk

# Enable nxdk-bundled SDL2 (required for SDL.h, gamepad, file I/O)
NXDK_SDL = y

# Source files
SRCS = $(wildcard src/*.c)

# Include directories
CFLAGS += -Iinclude

# Math library
LDFLAGS += -lm

# ISO content: assets/ dir copied into disc image (accessible at D:\)
ISO_ASSETS_DIR = assets
ISO_DIR        = $(ISO_ASSETS_DIR)

# Debug builds
ifdef DEBUG
CFLAGS += -g -DDEBUG
endif

# nxdk will handle the rest
include $(NXDK_DIR)/Makefile
