CC := clang++
# Added -Isrc/main to the include paths so test files can find main headers
CFLAGS := -Wall -Wextra -Iinclude -Isrc/main -I/opt/homebrew/include/eigen3 -DEIGEN_USE_BLAS -std=c++23 -MMD -MP
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

# `find` naturally searches recursively, grabbing all .cpp files in src/main and src/test.
SRCS := $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Define dependency files matching the object files
DEPS := $(OBJS:.o=.d)

LDFLAGS := -framework Accelerate

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
	@find $(BIN_DIR) -type f -name '._*' -delete || true
	@find $(OBJ_DIR) -type f -name '._*' -delete || true
	@find $(SRC_DIR) -type f -name '._*' -delete || true
	@remove-trackers

all: prebuild $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@ 
#	@cp toolchain/test-data/*.csv bin

# Adjusted to create nested directories dynamically before compiling
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $@

clean:
	@if [ -d $(OBJ_DIR) ]; then find $(OBJ_DIR) -type f ! -name '.gitkeep' -delete; fi
	@if [ -d $(BIN_DIR) ]; then find $(BIN_DIR) -type f ! -name '.gitkeep' -delete; fi

# Include the generated dependency files so Make knows when headers change
-include $(DEPS)