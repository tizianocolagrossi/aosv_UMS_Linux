CC := gcc
BUILDDIR := build
CFLAGS := -std=gnu11 -Wall -O3 -g
LIBS := -pthread
INC := -Iinclude

TARGET_BIN_DIR := bin
TARGET_LIB_DIR := lib
TARGET_LIB = umslib
TARGET_BIN = test

$(TARGET_BIN_DIR)/$(TARGET_BIN): $(BUILDDIR)/$(TARGET_BIN).o $(TARGET_LIB_DIR)/$(TARGET_LIB).a
	@echo "==> Compiling binary..."
	@mkdir -p $(TARGET_BIN_DIR) 
	@$(CC) $(CFLAGS) $(LIBS) $(INC) $^ -o $@

$(BUILDDIR)/$(TARGET_BIN).o: ./test.c
	@echo "==> Compiling binary object..."
	@mkdir -p $(BUILDDIR) 
	@$(CC) $(CFLAGS) $(LIBS) $(INC) -c $< -o $@

$(TARGET_LIB_DIR)/$(TARGET_LIB).a: $(BUILDDIR)/$(TARGET_LIB).o
	@echo "==> Packing library..."
	@mkdir -p $(TARGET_LIB_DIR) 
	@ar rcs $@ $^

$(BUILDDIR)/$(TARGET_LIB).o: ./ums.c ./*.h
	@echo "==> Compiling library object..."
	@mkdir -p $(BUILDDIR) 
	@$(CC) $(CFLAGS) $(LIBS) $(INC) -c -o $@ $<

lib: $(TARGET_LIB_DIR)/$(TARGET_LIB).a
bin: $(TARGET_BIN_DIR)/$(TARGET_BIN)

clean:
	rm -rvf $(TARGET_BIN_DIR) $(TARGET_LIB_DIR) $(BUILDDIR)