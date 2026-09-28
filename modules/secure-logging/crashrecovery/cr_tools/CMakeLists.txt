# File: modules/secure-logging/crashrecovery/cr_tools/CMakeLists.txt

# -- Executable Target (cr_log_count) ---

set(CR_LOG_COUNT_EXE_SOURCES cr_log_count.c)

add_executable(cr_log_count ${CR_LOG_COUNT_EXE_SOURCES})

target_compile_options(cr_log_count PRIVATE
    ${GLIB_CFLAGS}
)

target_include_directories(cr_log_count PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../cr_common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
)

target_link_libraries(cr_log_count PRIVATE
    cr_common_lib
    utils_slog
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    m
)

# -- Install Rules ---

install(TARGETS cr_log_count
    RUNTIME DESTINATION bin
    COMPONENT cr_log_count
)