#!/bin/bash

# Build script for OVPN Client (GTK3 + NetworkManager)
# Author: Generated for C GTK3 OpenVPN Client with NetworkManager integration
# Usage: ./build.sh [options]

set -e  # Exit on any error

# Script configuration
SCRIPT_NAME="build.sh"
PROJECT_NAME="ovpn-client"
SOURCE_FILE="ovpn_client.c"
BUILD_DIR="build"
LOG_FILE="build.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1" | tee -a "$LOG_FILE"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_FILE"
}

# Function to show usage
show_usage() {
    echo "Usage: $SCRIPT_NAME [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -c, --clean         Clean build artifacts before building"
    echo "  -d, --debug         Build in debug mode"
    echo "  -i, --install       Install after building"
    echo "  -r, --run           Run after building"
    echo "  -t, --test          Test compilation only"
    echo "  -v, --verbose       Verbose output"
    echo "  --check-deps        Check dependencies only"
    echo "  --install-deps      Install missing dependencies (Ubuntu/Debian)"
    echo "  --package           Create source package"
    echo ""
    echo "Examples:"
    echo "  $SCRIPT_NAME                    # Normal build"
    echo "  $SCRIPT_NAME --clean --debug    # Clean debug build"
    echo "  $SCRIPT_NAME --install --run    # Build, install and run"
    echo "  $SCRIPT_NAME --install-deps     # Install dependencies"
}

# Function to install dependencies
install_dependencies() {
    log_info "Installing dependencies..."
    
    if command -v apt-get >/dev/null 2>&1; then
        log_info "Detected Debian/Ubuntu system"
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            pkg-config \
            libgtk-3-dev \
            libnm-dev \
            libayatana-appindicator3-dev \
            uuid-dev \
            libglib2.0-dev
        log_success "Dependencies installed successfully"
    elif command -v dnf >/dev/null 2>&1; then
        log_info "Detected Fedora/RHEL system"
        sudo dnf install -y \
            gcc \
            pkg-config \
            gtk3-devel \
            NetworkManager-libnm-devel \
            libayatana-appindicator3-devel \
            libuuid-devel \
            glib2-devel
        log_success "Dependencies installed successfully"
    elif command -v pacman >/dev/null 2>&1; then
        log_info "Detected Arch Linux system"
        sudo pacman -S --needed \
            base-devel \
            pkgconf \
            gtk3 \
            networkmanager \
            libayatana-appindicator \
            util-linux \
            glib2
        log_success "Dependencies installed successfully"
    else
        log_error "Unsupported package manager. Please install dependencies manually."
        return 1
    fi
}

# Function to check dependencies
check_dependencies() {
    log_info "Checking dependencies..."
    
    local missing_deps=0
    
    # Check for required tools
    if ! command -v gcc >/dev/null 2>&1; then
        log_error "GCC compiler not found"
        log_info "Install with: sudo apt-get install build-essential (Ubuntu/Debian)"
        log_info "            or: sudo dnf install gcc (Fedora/RHEL)"
        log_info "            or: sudo pacman -S gcc (Arch Linux)"
        missing_deps=$((missing_deps + 1))
    else
        log_success "GCC compiler found: $(gcc --version | head -n1)"
    fi
    
    if ! command -v pkg-config >/dev/null 2>&1; then
        log_error "pkg-config not found"
        log_info "Install with: sudo apt-get install pkg-config (Ubuntu/Debian)"
        log_info "            or: sudo dnf install pkgconfig (Fedora/RHEL)"
        log_info "            or: sudo pacman -S pkgconf (Arch Linux)"
        missing_deps=$((missing_deps + 1))
    else
        log_success "pkg-config found"
    fi
    
    # Check for GTK3
    if ! pkg-config --exists gtk+-3.0 2>/dev/null; then
        log_error "GTK3 development libraries not found"
        log_info "Install with: sudo apt-get install libgtk-3-dev (Ubuntu/Debian)"
        log_info "            or: sudo dnf install gtk3-devel (Fedora/RHEL)"
        log_info "            or: sudo pacman -S gtk3 (Arch Linux)"
        missing_deps=$((missing_deps + 1))
    else
        local gtk_version=$(pkg-config --modversion gtk+-3.0 2>/dev/null)
        log_success "GTK3 found: version $gtk_version"
    fi
    
    # Check for NetworkManager library
    if ! pkg-config --exists libnm 2>/dev/null; then
        log_error "NetworkManager development libraries not found"
        log_info "Install with: sudo apt-get install libnm-dev (Ubuntu/Debian)"
        log_info "            or: sudo dnf install NetworkManager-libnm-devel (Fedora/RHEL)"
        log_info "            or: sudo pacman -S networkmanager (Arch Linux)"
        missing_deps=$((missing_deps + 1))
    else
        local nm_version=$(pkg-config --modversion libnm 2>/dev/null)
        log_success "NetworkManager found: version $nm_version"
    fi
    
    # Check for AppIndicator library
    if ! pkg-config --exists ayatana-appindicator3-0.1 2>/dev/null; then
        log_error "Ayatana AppIndicator development libraries not found"
        log_info "Install with: sudo apt-get install libayatana-appindicator3-dev (Ubuntu/Debian)"
        log_info "            or: sudo dnf install libayatana-appindicator3-devel (Fedora/RHEL)"
        log_info "            or: sudo pacman -S libayatana-appindicator (Arch Linux)"
        missing_deps=$((missing_deps + 1))
    else
        local indicator_version=$(pkg-config --modversion ayatana-appindicator3-0.1 2>/dev/null)
        log_success "Ayatana AppIndicator found: version $indicator_version"
    fi
    
    # Check for UUID library
    if ! ldconfig -p 2>/dev/null | grep -q libuuid; then
        log_error "UUID development libraries not found"
        log_info "Install with: sudo apt-get install uuid-dev (Ubuntu/Debian)"
        log_info "            or: sudo dnf install libuuid-devel (Fedora/RHEL)"
        log_info "            or: sudo pacman -S util-linux (Arch Linux)"
        missing_deps=$((missing_deps + 1))
    else
        log_success "UUID library found"
    fi
    
    # Check for GLib
    if ! pkg-config --exists glib-2.0 2>/dev/null; then
        log_error "GLib development libraries not found"
        log_info "Usually included with GTK3 development packages"
        missing_deps=$((missing_deps + 1))
    else
        local glib_version=$(pkg-config --modversion glib-2.0 2>/dev/null)
        log_success "GLib found: version $glib_version"
    fi
    
    if [ $missing_deps -gt 0 ]; then
        log_error "$missing_deps dependencies missing"
        log_info "Run './build.sh --install-deps' to install dependencies automatically"
        return 1
    else
        log_success "All dependencies satisfied"
        return 0
    fi
}

# Function to clean build artifacts
clean_build() {
    log_info "Cleaning build artifacts..."
    rm -f "$PROJECT_NAME" "${PROJECT_NAME}-debug" 2>/dev/null || true
    rm -f *.o 2>/dev/null || true
    rm -rf "$BUILD_DIR" 2>/dev/null || true
    rm -f /tmp/ovpn_importer.log 2>/dev/null || true
    log_success "Clean completed"
}

# Function to build the project
build_project() {
    local build_type="$1"
    
    log_info "Starting build process..."
    
    # Check if source file exists
    if [ ! -f "$SOURCE_FILE" ]; then
        log_error "Source file '$SOURCE_FILE' not found"
        return 1
    fi
    
    # Get all required flags using pkg-config
    local gtk_cflags=$(pkg-config --cflags gtk+-3.0 2>/dev/null)
    local gtk_libs=$(pkg-config --libs gtk+-3.0 2>/dev/null)
    local nm_cflags=$(pkg-config --cflags libnm 2>/dev/null)
    local nm_libs=$(pkg-config --libs libnm 2>/dev/null)
    local indicator_cflags=$(pkg-config --cflags ayatana-appindicator3-0.1 2>/dev/null)
    local indicator_libs=$(pkg-config --libs ayatana-appindicator3-0.1 2>/dev/null)
    
    local all_cflags="$gtk_cflags $nm_cflags $indicator_cflags"
    local all_libs="$gtk_libs $nm_libs $indicator_libs -luuid -lm"
    
    if [ -z "$gtk_cflags" ] || [ -z "$gtk_libs" ]; then
        log_error "Failed to get GTK3 compiler flags"
        return 1
    fi
    
    if [ -z "$nm_cflags" ] || [ -z "$nm_libs" ]; then
        log_error "Failed to get NetworkManager compiler flags"
        return 1
    fi
    
    # Build based on type
    if [ "$build_type" = "debug" ]; then
        log_info "Building in debug mode..."
        local cmd="gcc -Wall -Wextra -std=c99 -g -DDEBUG $all_cflags -o ${PROJECT_NAME}-debug $SOURCE_FILE $all_libs"
        log_info "Build command: $cmd"
        if eval $cmd; then
            log_success "Debug build completed successfully"
        else
            log_error "Debug build failed"
            return 1
        fi
    else
        log_info "Building in release mode..."
        local cmd="gcc -Wall -Wextra -std=c99 -O2 $all_cflags -o $PROJECT_NAME $SOURCE_FILE $all_libs"
        log_info "Build command: $cmd"
        if eval $cmd; then
            log_success "Release build completed successfully"
        else
            log_error "Release build failed"
            return 1
        fi
    fi
    
    return 0
}

# Function to test compilation
test_compilation() {
    log_info "Testing compilation..."
    
    # Check if source file exists
    if [ ! -f "$SOURCE_FILE" ]; then
        log_error "Source file '$SOURCE_FILE' not found"
        return 1
    fi
    
    # Get all required flags
    local all_cflags=$(pkg-config --cflags gtk+-3.0 libnm ayatana-appindicator3-0.1 2>/dev/null)
    
    if [ -z "$all_cflags" ]; then
        log_error "Failed to get required compiler flags"
        return 1
    fi
    
    local cmd="gcc -Wall -Wextra -std=c99 -O2 $all_cflags -c $SOURCE_FILE -o test.o"
    log_info "Test command: $cmd"
    
    if eval $cmd; then
        log_success "Compilation test passed"
        rm -f test.o
        return 0
    else
        log_error "Compilation test failed"
        return 1
    fi
}

# Function to install the application
install_app() {
    log_info "Installing application..."
    
    if [ ! -f "$PROJECT_NAME" ]; then
        log_error "Executable '$PROJECT_NAME' not found. Build first."
        return 1
    fi
    
    if sudo cp "$PROJECT_NAME" /usr/local/bin/; then
        log_success "Installation completed to /usr/local/bin/$PROJECT_NAME"
        return 0
    else
        log_error "Installation failed"
        return 1
    fi
}

# Function to run the application
run_app() {
    local build_type="$1"
    
    # Check if we're in a GUI environment
    if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
        log_warning "No display environment detected. Make sure you're running in a GUI environment."
        log_info "Try: export DISPLAY=:0 (for X11) or check Wayland setup"
    fi
    
    if [ "$build_type" = "debug" ]; then
        log_info "Running application in debug mode..."
        if [ -f "${PROJECT_NAME}-debug" ]; then
            ./"${PROJECT_NAME}-debug"
        else
            log_error "Debug executable not found"
            return 1
        fi
    else
        log_info "Running application..."
        if [ -f "$PROJECT_NAME" ]; then
            ./"$PROJECT_NAME"
        else
            log_error "Executable not found"
            return 1
        fi
    fi
}

# Function to create package
create_package() {
    log_info "Creating source package..."
    
    local files="$SOURCE_FILE Makefile build.sh"
    if [ -f "README.md" ]; then
        files="$files README.md"
    fi
    
    if tar -czf "${PROJECT_NAME}-src.tar.gz" $files 2>/dev/null; then
        log_success "Package created: ${PROJECT_NAME}-src.tar.gz"
        return 0
    else
        log_error "Package creation failed"
        return 1
    fi
}

# Main script logic
main() {
    local clean_first=false
    local debug_build=false
    local install_after=false
    local run_after=false
    local test_only=false
    local verbose=false
    local check_deps_only=false
    local install_deps_only=false
    local package_only=false
    
    # Initialize log file
    echo "Build started at $(date)" > "$LOG_FILE"
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -c|--clean)
                clean_first=true
                shift
                ;;
            -d|--debug)
                debug_build=true
                shift
                ;;
            -i|--install)
                install_after=true
                shift
                ;;
            -r|--run)
                run_after=true
                shift
                ;;
            -t|--test)
                test_only=true
                shift
                ;;
            -v|--verbose)
                verbose=true
                shift
                ;;
            --check-deps)
                check_deps_only=true
                shift
                ;;
            --install-deps)
                install_deps_only=true
                shift
                ;;
            --package)
                package_only=true
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Enable verbose mode if requested
    if [ "$verbose" = true ]; then
        set -x
    fi
    
    log_info "Starting $PROJECT_NAME build script"
    log_info "Build configuration:"
    log_info "  Clean first: $clean_first"
    log_info "  Debug build: $debug_build"
    log_info "  Install after: $install_after"
    log_info "  Run after: $run_after"
    log_info "  Test only: $test_only"
    
    # Install dependencies if requested
    if [ "$install_deps_only" = true ]; then
        install_dependencies
        exit $?
    fi
    
    # Check dependencies first
    if ! check_dependencies; then
        log_error "Dependency check failed"
        exit 1
    fi
    
    # If only checking dependencies, exit here
    if [ "$check_deps_only" = true ]; then
        exit 0
    fi
    
    # If only creating package, do that and exit
    if [ "$package_only" = true ]; then
        create_package
        exit $?
    fi
    
    # Clean if requested
    if [ "$clean_first" = true ]; then
        clean_build
    fi
    
    # Test compilation only if requested
    if [ "$test_only" = true ]; then
        test_compilation
        exit $?
    fi
    
    # Build the project
    local build_type="release"
    if [ "$debug_build" = true ]; then
        build_type="debug"
    fi
    
    if ! build_project "$build_type"; then
        log_error "Build failed"
        exit 1
    fi
    
    # Install if requested
    if [ "$install_after" = true ]; then
        if ! install_app; then
            log_error "Installation failed"
            exit 1
        fi
    fi
    
    # Run if requested
    if [ "$run_after" = true ]; then
        run_app "$build_type"
    fi
    
    log_success "Build script completed successfully"
    log_info "Log file: $LOG_FILE"
    
    # Show next steps
    echo ""
    log_info "Next steps:"
    log_info "  Run the application: ./$PROJECT_NAME"
    log_info "  Install system-wide: ./build.sh --install"
    log_info "  Clean build files: ./build.sh --clean"
    log_info "  View help: ./build.sh --help"
    echo ""
    log_info "GUI Troubleshooting:"
    log_info "  Check display: echo \$DISPLAY"
    log_info "  Run in debug mode: ./build.sh --debug --run"
    log_info "  Check log file: /tmp/ovpn_importer.log"
}

# Run main function with all arguments
main "$@"