# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

The project uses CMake for cross-platform builds. All executables are organized into categories:

### Building the Project
```bash
# Create build directory and configure
mkdir -p build && cd build
cmake ..

# Build all targets
make -j$(nproc)

# Build specific category
make server_udp_serve_toupper  # Server executable
make client_udp_client          # Client executable
make common_socket_init         # Common utility
```

### Build Output Structure
```
build/bin/
├── client/   # Client executables
├── server/   # Server executables
└── common/   # Common utilities
```

### Running Tests
```bash
# Run all tests
cd build && ctest

# Run specific test
./bin/test_<name>
```

## Architecture Overview

### Module Organization
The codebase implements network programming examples with UDP/TCP functionality, organized into three main modules:

1. **Common** (`src/common/`): Cross-platform socket initialization and utilities
   - `socket_init.c`: Platform-specific socket API initialization
   - `time_server*.c`: Time server implementations (IPv4/IPv6/dual-stack)
   - `addr_list.c`: Address list management utilities

2. **Client** (`src/client/`): UDP client implementations
   - Each file is a standalone executable demonstrating specific UDP operations
   - Programs include sendto, recvfrom, and echo client examples

3. **Server** (`src/server/`): UDP server implementations
   - Mirror implementations of client programs from server perspective
   - Note: Currently contains duplicated code from client directory

### Cross-Platform Abstraction
The codebase uses preprocessor macros for Windows/Unix compatibility:
- Windows: Uses Winsock2 API
- Unix/Linux: Uses BSD sockets
- Key macros defined in `include/client/udp.h` and `include/server/udp.h`:
  - `ISVALIDSOCKET()`, `CLOSESOCKET()`, `GETSOCKETERRNO()`

### Critical Security Considerations
The analysis report (`docs/sc-init.md`) identifies critical buffer overflow vulnerabilities in `udp_serve_toupper.c`. When working with this code:
1. Always null-terminate buffers after `recvfrom()`
2. Validate all command-line arguments
3. Use bounds checking for all buffer operations
4. Never trust network input

### Known Issues
- **Code Duplication**: ~30% duplicate code between client/server directories
- **Hard-coded Values**: Port 8080 and buffer sizes are magic numbers throughout
- **Missing Validation**: No input validation on command-line arguments or network data
- **Single-threaded**: All servers are single-threaded with synchronous I/O

## Development Guidelines

### When Adding New Network Programs
1. Place in appropriate directory (client/server/common)
2. Follow existing naming conventions: `<protocol>_<function>.c`
3. Use the platform abstraction macros from `udp.h`
4. Include proper error handling with `GETSOCKETERRNO()`

### Before Committing
1. Ensure no buffer overflow vulnerabilities
2. Add null termination after all `recvfrom()` calls
3. Validate all inputs (command-line and network)
4. Test on both Windows and Unix platforms if possible

### Priority Improvements
1. Fix buffer overflow in `udp_serve_toupper.c:88-89`
2. Consolidate duplicate UDP implementations
3. Create shared constants for ports and buffer sizes
4. Add comprehensive input validation