# OVPN Client (GTK3)

A simplified OpenVPN client GUI application built with C and GTK3.

## Features

- Import and parse .ovpn configuration files
- Display OpenVPN configuration analysis
- Simple GUI interface for VPN management
- Logging functionality
- Cross-platform compatibility (Linux focus)

## Dependencies

### Required packages:
- GCC compiler
- GTK3 development libraries
- GLib development libraries
- pkg-config

### Installation on different systems:

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install build-essential libgtk-3-dev pkg-config libnm-dev
```

**Fedora/RHEL/CentOS:**
```bash
sudo dnf install gcc gtk3-devel pkgconfig
# or for older versions:
sudo yum install gcc gtk3-devel pkgconfig
```

**Arch Linux:**
```bash
sudo pacman -S base-devel gtk3 pkgconf
```

## Building

### Using the build script (recommended):
```bash
# Make the build script executable
chmod +x build.sh

# Check dependencies
./build.sh --check-deps

# Normal build
./build.sh

# Clean debug build
./build.sh --clean --debug

# Build, install and run
./build.sh --install --run
```

### Using Makefile directly:
```bash
# Check dependencies
make check-deps

# Build
make

# Debug build
make debug

# Install
sudo make install

# Run
make run
```

### Manual compilation:
```bash
gcc -Wall -Wextra -std=c99 -O2 `pkg-config --cflags gtk+-3.0` \
    -o ovpn-client ovpn_client.c `pkg-config --libs gtk+-3.0` -lm
```

## Usage

1. Run the application:
   ```bash
   ./ovpn-client
   ```

2. Click "Import .ovpn File" to select an OpenVPN configuration file

3. The application will parse and display the configuration details

4. Use the interface to manage VPN connections (simplified demo version)

## Build Options

### Makefile targets:
- `make all` - Build the application (default)
- `make debug` - Build with debug symbols
- `make install` - Install to /usr/local/bin
- `make uninstall` - Remove from /usr/local/bin
- `make clean` - Remove build artifacts
- `make check-deps` - Check if dependencies are installed
- `make info` - Show build information
- `make run` - Build and run the application
- `make run-debug` - Build and run in debug mode
- `make test-compile` - Test compilation without linking
- `make package` - Create source tarball
- `make help` - Show available targets

### Build script options:
- `-h, --help` - Show help message
- `-c, --clean` - Clean build artifacts before building
- `-d, --debug` - Build in debug mode
- `-i, --install` - Install after building
- `-r, --run` - Run after building
- `-t, --test` - Test compilation only
- `-v, --verbose` - Verbose output
- `--check-deps` - Check dependencies only
- `--package` - Create source package

## File Structure

```
.
├── ovpn_client.c       # Main source file
├── Makefile           # Build configuration
├── build.sh           # Build script
├── README.md          # This file
└── build.log          # Build log (generated)
```

## Logging

The application creates a log file at `/tmp/ovpn_importer.log` for debugging purposes.

## Notes

- This is a simplified demonstration version
- Actual VPN connection functionality requires NetworkManager integration
- The application focuses on configuration parsing and GUI demonstration
- For production use, additional security and error handling should be implemented

## Troubleshooting

### Common issues:

1. **GTK3 not found:**
   - Install GTK3 development packages for your distribution
   - Verify with: `pkg-config --exists gtk+-3.0`

2. **Compilation errors:**
   - Check GCC version: `gcc --version`
   - Verify all dependencies: `./build.sh --check-deps`

3. **Runtime issues:**
   - Check log file: `/tmp/ovpn_importer.log`
   - Run in debug mode: `make run-debug`

## License

This project is provided as-is for educational and demonstration purposes.
