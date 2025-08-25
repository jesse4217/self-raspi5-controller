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

# Build multi-camera UDP system
make pi_zero_time_relay_server  # Central relay server
make pi_zero_time_main_client   # Main client interface
make pi_zero_time_sub_client    # Raspberry Pi sub-clients
```

### Build Output Structure
```
build/bin/
├── client/         # Basic client executables
├── server/         # Basic server executables
├── common/         # Common utilities
├── hq-cam-controller/  # HQ camera controller
└── pi-zero-libcam/ # Multi-camera UDP system (NEW)
    ├── time_relay_server    # Central relay server
    ├── time_main_client     # Main client interface
    ├── time_sub_client      # Raspberry Pi sub-clients
    ├── pi_client           # Original Pi client
    └── pi_server           # Original Pi server
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
The codebase implements network programming examples with UDP/TCP functionality, organized into modular components:

1. **Common** (`src/common/`): Cross-platform socket utilities (REORGANIZED)
   - `timer/`: Time server implementations (IPv4/IPv6/dual-stack)
     - `socket_init.c`: Platform-specific socket API initialization
     - `time_server*.c`: Various time server implementations
     - `addr_list.c`: Address list management utilities
   - `client/`: Basic UDP/TCP client implementations
   - `server/`: Basic UDP/TCP server implementations

2. **Multi-Camera System** (`src/multi-cam/`): **NEW** - Advanced UDP relay system
   - `time_protocol.h`: Protocol definitions and message types
   - `time_relay_server.c`: Central relay server with client registry
   - `time_main_client.c`: Interactive main client with number commands
   - `time_sub_client.c`: Raspberry Pi sub-clients with remote command execution
   - `pi_client.c` / `pi_server.c`: Original Pi implementations

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
- **Code Duplication**: ~30% duplicate code between client/server directories (partially resolved)
- **Hard-coded Values**: Port 8080 and buffer sizes are magic numbers throughout
- **Missing Validation**: No input validation on command-line arguments or network data (improved in multi-cam)
- **Single-threaded**: All servers are single-threaded with synchronous I/O (addressed with select() in multi-cam)

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

## Coding Standards

### Output and Logging Rules
- **No emojis or icons**: All console output, logging, and user messages must use plain text only
- Use text prefixes instead of emojis: `SUCCESS`, `ERROR`, `WARNING`, `[ONLINE]`, `[OFFLINE]`
- Keep output clean and terminal-friendly for all environments