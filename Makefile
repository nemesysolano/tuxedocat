CC := clang++
CFLAGS := -Wall -Wextra -Iinclude -O3 -I/opt/homebrew/include/eigen3 -DEIGEN_USE_BLAS -std=c++23 
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
SRCS := $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
LDFLAGS := -framework Accelerate

ifneq ($(filter 1,$(if $(__TEST_MAIN__),1)$(if $(__CLI_MAIN__),1)),)
    TARGET := $(BIN_DIR)/tuxedocat    
    $(if $(__TEST_MAIN__),$(eval CFLAGS += -D__TEST_MAIN__))
    $(if $(__CLI_MAIN__),$(eval CFLAGS += -D__CLI_MAIN__))
else
    TARGET := $(BIN_DIR)/libtuxedocat.dylib
    CFLAGS += -fPIC
    LDFLAGS += -shared 
endif

ifdef __DEBUG__
	CFLAGS += -D__DEBUG__
	LDFLAGS += -g 
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
	@cp toolchain/test-data/*.csv bin

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	@if [ -d $(OBJ_DIR) ]; then find $(OBJ_DIR) -type f ! -name '.gitkeep' -delete; fi
	@if [ -d $(BIN_DIR) ]; then find $(BIN_DIR) -type f ! -name '.gitkeep' -delete; fi

.PHONY: all clean