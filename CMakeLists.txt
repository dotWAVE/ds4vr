cmake_minimum_required(VERSION 3.24)
project(ds4vr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)

# OpenVR: headers-only consumption. Point SOURCE_SUBDIR at a nonexistent
# directory so FetchContent populates the source tree without running
# OpenVR's own CMakeLists (which would build the openvr_api shared lib we
# don't need — drivers only consume the headers).
FetchContent_Declare(
    openvr
    GIT_REPOSITORY https://github.com/ValveSoftware/openvr.git
    GIT_TAG        v2.5.1
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  cmake_skip_openvr_build
)
FetchContent_MakeAvailable(openvr)

add_library(openvr_driver_headers INTERFACE)
target_include_directories(openvr_driver_headers INTERFACE
    "${openvr_SOURCE_DIR}/headers"
)

# hidapi (libusb/hidapi). Static-link so we don't have to ship a separate
# hidapi.dll alongside the driver.
set(BUILD_SHARED_LIBS    OFF CACHE BOOL "" FORCE)
set(HIDAPI_WITH_TESTS    OFF CACHE BOOL "" FORCE)
set(HIDAPI_BUILD_HIDTEST OFF CACHE BOOL "" FORCE)
set(HIDAPI_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    hidapi
    GIT_REPOSITORY https://github.com/libusb/hidapi.git
    GIT_TAG        hidapi-0.14.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(hidapi)

add_library(driver_ds4vr SHARED
    src/dllmain.cpp
    src/server_driver.cpp
    src/touch_device.cpp
    src/hid_device.cpp
    src/ds4_parser.cpp
    src/dualsense_parser.cpp
    src/imu_fusion.cpp
    src/engagement.cpp
    src/config.cpp
)

target_link_libraries(driver_ds4vr PRIVATE
    openvr_driver_headers
    hidapi::hidapi
)

# config.cpp uses GetModuleHandleExW / GetModuleFileNameW / WideCharToMultiByte.
# These live in kernel32.lib which is linked implicitly by MSVC, but be explicit.
if(WIN32)
    target_link_libraries(driver_ds4vr PRIVATE kernel32)
endif()

target_include_directories(driver_ds4vr PRIVATE src)

set_target_properties(driver_ds4vr PROPERTIES
    OUTPUT_NAME "driver_ds4vr"
    PREFIX ""
)

if(WIN32)
    target_compile_definitions(driver_ds4vr PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    if(MSVC)
        # /wd4100: OpenVR's pure-virtual interface headers don't name their
        # parameters in declarations, so /W4 floods C4100 from openvr_driver.h.
        # Suppressing it project-wide is fine — our own code uses /*param*/ to
        # silence intentionally-unused arguments, so we don't lose coverage.
        target_compile_options(driver_ds4vr PRIVATE /W4 /permissive- /wd4100)
    endif()
endif()

# --- Stage a complete driver tree next to the build output -------------------
# After build: ${CMAKE_BINARY_DIR}/ds4vr is a self-contained tree that
# vrpathreg.exe can register directly (scripts/install.ps1 does this).

set(DS4VR_STAGE_DIR  "${CMAKE_BINARY_DIR}/ds4vr")
set(DS4VR_BIN_SUBDIR "bin/win64")

add_custom_command(TARGET driver_ds4vr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${DS4VR_STAGE_DIR}/${DS4VR_BIN_SUBDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/driver/ds4vr" "${DS4VR_STAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:driver_ds4vr>"
        "${DS4VR_STAGE_DIR}/${DS4VR_BIN_SUBDIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/ds4vr.ini"
        "${DS4VR_STAGE_DIR}/ds4vr.ini"
    COMMENT "Staging ds4vr driver tree at ${DS4VR_STAGE_DIR}"
    VERBATIM
)
