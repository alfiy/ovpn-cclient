# Makefile for OVPN Client (modular version)
PROJECT_NAME = ovpn-client
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
DEBUG_CFLAGS = -Wall -Wextra -std=c99 -g -DDEBUG

GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)
NM_CFLAGS = $(shell pkg-config --cflags libnm)
NM_LIBS = $(shell pkg-config --libs libnm)
INDICATOR_CFLAGS = $(shell pkg-config --cflags ayatana-appindicator3-0.1)
INDICATOR_LIBS = $(shell pkg-config --libs ayatana-appindicator3-0.1)
EXTRA_LIBS = -luuid -lm

ALL_CFLAGS = $(CFLAGS) $(GTK_CFLAGS) $(NM_CFLAGS) $(INDICATOR_CFLAGS) -MMD -MP -I$(SRC_DIR)
ALL_LIBS = $(GTK_LIBS) $(NM_LIBS) $(INDICATOR_LIBS) $(EXTRA_LIBS)

DEBUG_ALL_CFLAGS = $(DEBUG_CFLAGS) $(GTK_CFLAGS) $(NM_CFLAGS) $(INDICATOR_CFLAGS) -MMD -MP -I$(SRC_DIR)

TARGET = $(BUILD_DIR)/$(PROJECT_NAME)
TARGET_DEBUG = $(BUILD_DIR)/$(PROJECT_NAME)-debug

.PHONY: all debug install uninstall clean check-deps info run run-debug test-compile package help

all: $(TARGET)

debug: $(TARGET_DEBUG)

$(BUILD_DIR):
	mkdir -p $@

# Pattern rule for building .o files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	@echo "Linking $@..."
	$(CC) $(OBJS) -o $@ $(ALL_LIBS)
	@echo "Build completed successfully! ($@)"

$(TARGET_DEBUG): $(BUILD_DIR)
	$(CC) $(DEBUG_ALL_CFLAGS) $(SRCS) -o $@ $(ALL_LIBS)
	@echo "Debug build completed successfully! ($@)"

# Install target
install: $(TARGET)
	@echo "Installing $(PROJECT_NAME)..."
	sudo cp $(TARGET) /usr/local/bin/$(PROJECT_NAME)
	@echo "Installation completed!"

uninstall:
	@echo "Uninstalling $(PROJECT_NAME)..."
	sudo rm -f /usr/local/bin/$(PROJECT_NAME)
	@echo "Uninstallation completed!"

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f /tmp/ovpn_client.log /tmp/ovpn_importer.log
	@echo "Clean completed!"

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists gtk+-3.0 && echo "✓ GTK3 found" || echo "✗ GTK3 not found"
	@pkg-config --exists glib-2.0 && echo "✓ GLib found" || echo "✗ GLib not found"
	@pkg-config --exists libnm && echo "✓ NetworkManager found" || echo "✗ NetworkManager not found"
	@pkg-config --exists ayatana-appindicator3-0.1 && echo "✓ AppIndicator found" || echo "✗ AppIndicator not found"
	@which gcc >/dev/null 2>&1 && echo "✓ GCC found" || echo "✗ GCC not found"
	@echo "Checking additional libraries..."
	@ldconfig -p | grep -q libuuid && echo "✓ UUID library found" || echo "✗ UUID library not found"

info:
	@echo "=== Build Information ==="
	@echo "Project: $(PROJECT_NAME)"
	@echo "Sources: $(SRCS)"
	@echo "Target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "GTK3 CFLAGS: $(GTK_CFLAGS)"
	@echo "GTK3 LIBS: $(GTK_LIBS)"
	@echo "NetworkManager CFLAGS: $(NM_CFLAGS)"
	@echo "NetworkManager LIBS: $(NM_LIBS)"
	@echo "AppIndicator CFLAGS: $(INDICATOR_CFLAGS)"
	@echo "AppIndicator LIBS: $(INDICATOR_LIBS)"
	@echo "========================="

run: $(TARGET)
	@echo "Running $(PROJECT_NAME)..."
	./$(TARGET)

run-debug: debug
	@echo "Running $(PROJECT_NAME) in debug mode..."
	./$(TARGET_DEBUG)

test-compile:
	@echo "Testing compilation..."
	$(CC) $(ALL_CFLAGS) -c $(SRC_DIR)/main.c -o /tmp/test.o
	@echo "Compilation test passed!"
	rm -f /tmp/test.o

package:
	@echo "Creating source package..."
	tar -czf $(PROJECT_NAME)-src.tar.gz $(SRC_DIR)/*.c $(SRC_DIR)/*.h Makefile build.sh README.md 2>/dev/null || \
	tar -czf $(PROJECT_NAME)-src.tar.gz $(SRC_DIR)/*.c $(SRC_DIR)/*.h Makefile build.sh
	@echo "Package created: $(PROJECT_NAME)-src.tar.gz"

help:
	@echo "Available targets:"
	@echo "  all           - Build the application (default)"
	@echo "  debug         - Build with debug symbols"
	@echo "  install       - Install to /usr/local/bin"
	@echo "  uninstall     - Remove from /usr/local/bin"
	@echo "  clean         - Remove build artifacts"
	@echo "  check-deps    - Check if dependencies are installed"
	@echo "  info          - Show build information"
	@echo "  run           - Build and run the application"
	@echo "  run-debug     - Build and run in debug mode"
	@echo "  test-compile  - Test compilation without linking"
	@echo "  package       - Create source tarball"
	@echo "  help          - Show this help message"

-include $(DEPS)
