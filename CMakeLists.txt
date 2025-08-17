cmake_minimum_required(VERSION 3.10)
project(NetworkApp VERSION 1.0.0 LANGUAGES C CXX)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    # Common flags
    add_compile_options(-Wall -Wextra -Wpedantic)
    
    # Debug flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -DDEBUG)
    endif()
    
    # Release flags
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

# Platform-specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 and later
    set(PLATFORM_LIBS ws2_32)
elseif(UNIX)
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Create output directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/server)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/client)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/common)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/hq-cam-controller)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/inc)

# Common library and executables
file(GLOB_RECURSE COMMON_SOURCES "src/common/*.c" "src/common/*.cpp")
if(COMMON_SOURCES)
    # Build static library for common code
    add_library(common STATIC ${COMMON_SOURCES})
    target_link_libraries(common ${PLATFORM_LIBS})
    
    # Also build executables from common sources if they have main()
    foreach(COMMON_SOURCE ${COMMON_SOURCES})
        get_filename_component(COMMON_NAME ${COMMON_SOURCE} NAME_WE)
        # Use prefixed target name internally, but output with clean name
        add_executable(common_${COMMON_NAME} ${COMMON_SOURCE})
        target_link_libraries(common_${COMMON_NAME} ${PLATFORM_LIBS})
        set_target_properties(common_${COMMON_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/common
            OUTPUT_NAME ${COMMON_NAME})
    endforeach()
endif()

# Server executable
file(GLOB_RECURSE SERVER_SOURCES "src/server/*.c" "src/server/*.cpp")
if(SERVER_SOURCES)
    foreach(SERVER_SOURCE ${SERVER_SOURCES})
        get_filename_component(SERVER_NAME ${SERVER_SOURCE} NAME_WE)
        # Use prefixed target name internally, but output with clean name
        add_executable(server_${SERVER_NAME} ${SERVER_SOURCE})
        if(TARGET common)
            target_link_libraries(server_${SERVER_NAME} common ${PLATFORM_LIBS})
        else()
            target_link_libraries(server_${SERVER_NAME} ${PLATFORM_LIBS})
        endif()
        set_target_properties(server_${SERVER_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/server
            OUTPUT_NAME ${SERVER_NAME})
    endforeach()
endif()

# Client executable
file(GLOB_RECURSE CLIENT_SOURCES "src/client/*.c" "src/client/*.cpp")
if(CLIENT_SOURCES)
    foreach(CLIENT_SOURCE ${CLIENT_SOURCES})
        get_filename_component(CLIENT_NAME ${CLIENT_SOURCE} NAME_WE)
        # Use prefixed target name internally, but output with clean name
        add_executable(client_${CLIENT_NAME} ${CLIENT_SOURCE})
        if(TARGET common)
            target_link_libraries(client_${CLIENT_NAME} common ${PLATFORM_LIBS})
        else()
            target_link_libraries(client_${CLIENT_NAME} ${PLATFORM_LIBS})
        endif()
        set_target_properties(client_${CLIENT_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/client
            OUTPUT_NAME ${CLIENT_NAME})
    endforeach()
endif()

# HQ Camera Controller executables
file(GLOB_RECURSE HQ_CAM_SOURCES "src/hq-cam-controller/*.c" "src/hq-cam-controller/*.cpp")
if(HQ_CAM_SOURCES)
    foreach(HQ_CAM_SOURCE ${HQ_CAM_SOURCES})
        get_filename_component(HQ_CAM_NAME ${HQ_CAM_SOURCE} NAME_WE)
        # Use prefixed target name internally, but output with clean name
        add_executable(hq_cam_${HQ_CAM_NAME} ${HQ_CAM_SOURCE})
        if(TARGET common)
            target_link_libraries(hq_cam_${HQ_CAM_NAME} common ${PLATFORM_LIBS})
        else()
            target_link_libraries(hq_cam_${HQ_CAM_NAME} ${PLATFORM_LIBS})
        endif()
        set_target_properties(hq_cam_${HQ_CAM_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/hq-cam-controller
            OUTPUT_NAME ${HQ_CAM_NAME})
    endforeach()
endif()

# Examples
file(GLOB EXAMPLE_SOURCES "examples/*.c" "examples/*.cpp")
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(example_${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    if(TARGET common)
        target_link_libraries(example_${EXAMPLE_NAME} common ${PLATFORM_LIBS})
    else()
        target_link_libraries(example_${EXAMPLE_NAME} ${PLATFORM_LIBS})
    endif()
endforeach()

# Testing
enable_testing()
file(GLOB TEST_SOURCES "tests/*.c" "tests/*.cpp")
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(test_${TEST_NAME} ${TEST_SOURCE})
    if(TARGET common)
        target_link_libraries(test_${TEST_NAME} common ${PLATFORM_LIBS})
    else()
        target_link_libraries(test_${TEST_NAME} ${PLATFORM_LIBS})
    endif()
    add_test(NAME ${TEST_NAME} COMMAND test_${TEST_NAME})
endforeach()

# Installation rules
# Install all executables from bin subdirectories
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/ DESTINATION bin
        USE_SOURCE_PERMISSIONS
        PATTERN "*"
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                    GROUP_EXECUTE GROUP_READ
                    WORLD_EXECUTE WORLD_READ)
install(DIRECTORY inc/ DESTINATION include)

# Print configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Platform libs: ${PLATFORM_LIBS}")