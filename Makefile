LDFLAGS := 
CFLAGS := -Wall -Wextra -Wno-deprecated-declarations -DEIGEN_USE_BLAS -std=c++23 -MMD -MP -Iinclude -Isrc -Isrc/main 

# Windows builds target the active MSYS2/MinGW environment by default.
ifeq ($(OS),Windows_NT)
    WINDOWS_PREFIX ?= $(if $(MINGW_PREFIX),$(MINGW_PREFIX),/mingw64)
    CC := $(WINDOWS_PREFIX)/bin/g++.exe
    CFLAGS += -I$(WINDOWS_PREFIX)/include/eigen3 -I$(WINDOWS_PREFIX)/include
    BOOST_INCLUDE_DIRS ?= $(WINDOWS_PREFIX)/include
    LDFLAGS += -L$(WINDOWS_PREFIX)/lib -lopenblas -lodbc32 -lstdc++exp
    SHARED_EXT := .dll
    EXE_EXT := .exe
else ifeq ($(shell uname -s), Darwin)
    CC := clang++
    CFLAGS += -stdlib=libc++ -I/opt/homebrew/include/eigen3 -I/opt/homebrew/include -I/opt/homebrew/opt/unixodbc/include
    BOOST_INCLUDE_DIRS ?= /opt/homebrew/include /usr/local/include
else ifeq ($(shell uname -s), SunOS) 
    # Use native GCC 14 on Solaris (automatically handles its own headers and libstdc++)
    CC := /usr/gcc/14/bin/g++
    CFLAGS += -I/usr/local/include/eigen3 -Icontrib/mdspan/include/mdspan
    BOOST_INCLUDE_DIRS ?= /usr/local/include /usr/include
else
    CC := c++
    CFLAGS += -I/usr/include/eigen3
    BOOST_INCLUDE_DIRS ?= /usr/include /usr/local/include
endif

# Boost.Math is header-only; no Boost library needs to be linked.
CFLAGS += $(addprefix -I,$(BOOST_INCLUDE_DIRS))

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

SRCS := $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

-include $(DEPS)

ifeq ($(shell uname -s), Darwin)
    LDFLAGS += -framework Accelerate -L/opt/homebrew/lib -L/opt/homebrew/opt/unixodbc/lib -lodbc
else ifeq ($(OS),Windows_NT)
    # Windows libraries are configured in the platform block above.
else ifeq ($(shell uname -s), SunOS) 
    # g++ automatically links libstdc++; we only need to provide OpenBLAS here
    LDFLAGS += -L/usr/local/openblas/lib -Wl,-rpath,/usr/local/openblas/lib -lopenblas
else 
    LDFLAGS += -lopenblas 
endif

# Evaluate definitions and default to shared library if neither __TEST_MAIN__ nor __CLI_MAIN__ is provided
ifneq ($(filter 1,$(if $(__TEST_MAIN__),1)$(if $(__CLI_MAIN__),1)),)
    TARGET := $(BIN_DIR)/tuxedocat$(EXE_EXT)
    $(if $(__TEST_MAIN__),$(eval CFLAGS += -D__TEST_MAIN__)) #ok
    $(if $(__CLI_MAIN__),$(eval CFLAGS += -D__CLI_MAIN__))
else
    TARGET := $(BIN_DIR)/libtuxedocat$(if $(SHARED_EXT),$(SHARED_EXT),.dylib)
    ifneq ($(OS),Windows_NT)
        CFLAGS += -fPIC
    endif
    LDFLAGS += -shared 
endif

ifdef __DEBUG__
	CFLAGS += -D__DEBUG__ -O0
	LDFLAGS += -g 
else	
	CFLAGS += -O2
endif

.PHONY: all clean prebuild
.DEFAULT_GOAL := all

prebuild:
	@find $(BIN_DIR) -type f -name '._*' -exec rm -f {} + || true
	@find $(OBJ_DIR) -type f -name '._*' -exec rm -f {} + || true
	@find $(SRC_DIR) -type f -name '._*' -exec rm -f {} + || true

all: prebuild $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@ 

# Adjusted to create nested directories dynamically before compiling
ifeq ($(OS),Windows_NT)
$(OBJ_DIR)/nanodbc/nanodbc.o: CFLAGS += -Wno-int-to-pointer-cast
endif

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $@

clean:
	@if [ -d $(OBJ_DIR) ]; then find $(OBJ_DIR) -type f ! -name '.gitkeep' -exec rm -f {} +; fi
	@if [ -d $(BIN_DIR) ]; then find $(BIN_DIR) -type f ! -name '.gitkeep' -exec rm -f {} +; fi