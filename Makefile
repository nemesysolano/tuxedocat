# Default compiler for macOS / Ubuntu
CC := clang++

LDFLAGS := 
CFLAGS := -Wall -Wextra -Wno-deprecated-declarations -DEIGEN_USE_BLAS -std=c++23 -MMD -MP -Iinclude -Isrc/main 

ifeq ($(shell uname -s), Darwin)
    CC := clang++
    CFLAGS += -stdlib=libc++ -I/opt/homebrew/include/eigen3
else ifeq ($(shell uname -s), SunOS) 
    # Use native GCC 14 on Solaris (automatically handles its own headers and libstdc++)
    CC := /usr/gcc/14/bin/g++
    CFLAGS += -I/usr/local/include/eigen3 -Icontrib/mdspan/include/mdspan
else
    CC := c++
    CFLAGS += -I/usr/include/eigen3
endif

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

SRCS := $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

ifeq ($(shell uname -s), Darwin)
    LDFLAGS += -framework Accelerate
else ifeq ($(shell uname -s), SunOS) 
    # g++ automatically links libstdc++; we only need to provide OpenBLAS here
    LDFLAGS += -L/usr/local/openblas/lib -Wl,-rpath,/usr/local/openblas/lib -lopenblas
else 
    LDFLAGS += -lopenblas 
endif

# Evaluate definitions and default to shared library if neither __TEST_MAIN__ nor __CLI_MAIN__ is provided
ifneq ($(filter 1,$(if $(__TEST_MAIN__),1)$(if $(__CLI_MAIN__),1)),)
    TARGET := $(BIN_DIR)/tuxedocat    
    $(if $(__TEST_MAIN__),$(eval CFLAGS += -D__TEST_MAIN__)) #ok
    $(if $(__CLI_MAIN__),$(eval CFLAGS += -D__CLI_MAIN__))
else
    TARGET := $(BIN_DIR)/libtuxedocat.dylib # OK
    CFLAGS += -fPIC
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
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $@

clean:
	@if [ -d $(OBJ_DIR) ]; then find $(OBJ_DIR) -type f ! -name '.gitkeep' -exec rm -f {} +; fi
	@if [ -d $(BIN_DIR) ]; then find $(BIN_DIR) -type f ! -name '.gitkeep' -exec rm -f {} +; fi