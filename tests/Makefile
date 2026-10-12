# Host-side tests for the portable parts of 3dsFM (source/core). They build
# with the system compiler and run under AddressSanitizer and UBSan.
#
#   make -C tests check          run the unit tests
#   make -C tests SAN=           build without sanitizers

CC       ?= cc
CXX      ?= c++
ROOT     := ..
CORE     := $(ROOT)/source/core
BUILD    := build

SAN      ?= -fsanitize=address,undefined -fno-sanitize=pointer-overflow \
            -fno-sanitize-recover=undefined -fno-omit-frame-pointer
WARN     := -Wall -Wextra
CPPFLAGS := -I$(CORE)
CXXFLAGS := -std=c++17 -g -O1 $(WARN) -Werror $(SAN) $(CPPFLAGS)
LDLIBS   := -lm

CORE_SRC := $(wildcard $(CORE)/*.cpp)
UNIT_SRC := $(wildcard unit/*.cpp)
HEADERS  := $(wildcard $(CORE)/*.hpp *.hpp)

.PHONY: all check unit clean

all: $(BUILD)/unit_tests

check: unit

unit: $(BUILD)/unit_tests
	./$(BUILD)/unit_tests

$(BUILD)/unit_tests: $(UNIT_SRC) $(CORE_SRC) $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(UNIT_SRC) $(CORE_SRC) -o $@ $(LDLIBS)

clean:
	rm -rf $(BUILD)
