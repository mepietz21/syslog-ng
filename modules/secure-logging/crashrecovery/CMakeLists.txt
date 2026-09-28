# File: modules/secure-logging/crashrecovery/CMakeLists.txt

# Set minimum required CMake version
cmake_minimum_required(VERSION 3.10)
project(CrashRecovery C)

# Find necessary packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0 gobject-2.0)
find_package(OpenSSL REQUIRED)

set(LIBSYSLOG_SECURE_LOGGING_TARGET syslog-ng-secure-logging)

# Set common C flags, including the standard flags from the Makefile
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")

# Add subdirectories for building modules

add_subdirectory(cr_common)
add_subdirectory(cr_logger)
add_subdirectory(cr_verifier)
add_subdirectory(cr_destination)
add_subdirectory(cr_tools)

add_executable(cr_log_count
    cr_tools/cr_log_count.c
)

target_include_directories(cr_log_count PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/crashrecovery/cr_common
)

target_link_libraries(cr_log_count PRIVATE
    cr_common_lib
    ${GLIB_LIBRARIES}
)

install(TARGETS cr_log_count DESTINATION bin)
