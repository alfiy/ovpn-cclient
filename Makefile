# Makefile for OVPN Client (GTK3 + NetworkManager)
# Author: Generated for C GTK3 OpenVPN Client

# Project settings
PROJECT_NAME = ovpn-client
SOURCE_FILE = ovpn_client.c
TARGET = $(PROJECT_NAME)

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
DEBUG_CFLAGS = -Wall -Wextra -std=c99 -g -DDEBUG

# Package configurations
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

NM_CFLAGS = $(shell pkg-config --cflags libnm)
NM_LIBS = $(shell pkg-config --libs libnm)

INDICATOR_CFLAGS = $(shell pkg-config --cflags ayatana-appindicator3-0.1)
INDICATOR_LIBS = $(shell pkg-config --libs ayatana-appindicator3-0.1)

# Additional libraries
EXTRA_LIBS = -luuid -lm

# All compiler flags and libraries
ALL_CFLAGS = $(CFLAGS) $(GTK_CFLAGS) $(NM_CFLAGS) $(INDICATOR_CFLAGS)
ALL_LIBS = $(GTK_LIBS) $(NM_LIBS) $(INDICATOR_LIBS) $(EXTRA_LIBS)

# Debug build flags
DEBUG_ALL_CFLAGS = $(DEBUG_CFLAGS) $(GTK_CFLAGS) $(NM_CFLAGS) $(INDICATOR_CFLAGS)

# Default target
all: $(TARGET)

# Main target
$(TARGET): $(SOURCE_FILE)
	@echo "Building $(PROJECT_NAME)..."
	$(CC) $(ALL_CFLAGS) -o $(TARGET) $(SOURCE_FILE) $(ALL_LIBS)
	@echo "Build completed successfully!"

# Debug build
debug: $(SOURCE_FILE)
	@echo "Building $(PROJECT_NAME) in debug mode..."
	$(CC) $(DEBUG_ALL_CFLAGS) -o $(TARGET)-debug $(SOURCE_FILE) $(ALL_LIBS)
	@echo "Debug build completed successfully!"

# Install target
install: $(TARGET)
	@echo "Installing $(PROJECT_NAME)..."
	sudo cp $(TARGET) /usr/local/bin/
	@echo "Installation completed!"

# Uninstall target
uninstall:
	@echo "Uninstalling $(PROJECT_NAME)..."
	sudo rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstallation completed!"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET) $(TARGET)-debug
	rm -f *.o
	rm -f /tmp/ovpn_importer.log
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

# Show build information
info:
	@echo "=== Build Information ==="
	@echo "Project: $(PROJECT_NAME)"
	@echo "Source: $(SOURCE_FILE)"
	@echo "Target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "GTK3 CFLAGS: $(GTK_CFLAGS)"
	@echo "GTK3 LIBS: $(GTK_LIBS)"
	@echo "NetworkManager CFLAGS: $(NM_CFLAGS)"
	@echo "NetworkManager LIBS: $(NM_LIBS)"
	@echo "AppIndicator CFLAGS: $(INDICATOR_CFLAGS)"
	@echo "AppIndicator LIBS: $(INDICATOR_LIBS)"
	@echo "========================="

# Run the application
run: $(TARGET)
	@echo "Running $(PROJECT_NAME)..."
	./$(TARGET)

# Run in debug mode
run-debug: debug
	@echo "Running $(PROJECT_NAME) in debug mode..."
	./$(TARGET)-debug

# Test build (compile only, don't link)
test-compile:
	@echo "Testing compilation..."
	$(CC) $(ALL_CFLAGS) -c $(SOURCE_FILE) -o test.o
	@echo "Compilation test passed!"
	rm -f test.o

# Package source (create tarball)
package:
	@echo "Creating source package..."
	tar -czf $(PROJECT_NAME)-src.tar.gz $(SOURCE_FILE) Makefile build.sh README.md 2>/dev/null || \
	tar -czf $(PROJECT_NAME)-src.tar.gz $(SOURCE_FILE) Makefile build.sh
	@echo "Package created: $(PROJECT_NAME)-src.tar.gz"

# Help target
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

# Phony targets
.PHONY: all debug install uninstall clean check-deps info run run-debug test-compile package help