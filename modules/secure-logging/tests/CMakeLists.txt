# File: modules/secure-logging/tests/CMakeLists.txt

add_unit_test(
    LIBTEST CRITERION 
    TARGET test_secure_logging 
    DEPENDS 
        secure-logging 
        syslog-ng-secure-logging
        syslogformat
        cr_logger_lib
        cr_common_lib
        cr_verifier_lib
        cr_destination_lib
)

target_include_directories(test_secure_logging PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..                       # For utils_slog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_destination
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_logger
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_common
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_verifier
)

add_unit_test(
    LIBTEST CRITERION 
    TARGET test_cr_common 
    DEPENDS 
        secure-logging 
        syslog-ng-secure-logging
        syslogformat
        cr_logger_lib
        cr_common_lib
        cr_verifier_lib
        cr_destination_lib
)

target_include_directories(test_cr_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..                       # For utils_slog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_destination
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_logger
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_common
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_verifier
)

add_unit_test(
    LIBTEST CRITERION 
    TARGET test_cr_destination 
    DEPENDS 
        secure-logging 
        syslog-ng-secure-logging
        syslogformat
        cr_logger_lib
        cr_common_lib
        cr_verifier_lib
        cr_destination_lib
)

target_include_directories(test_cr_destination PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..                       # For utils_slog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_destination
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_logger
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_common
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_verifier
)

add_unit_test(
    LIBTEST CRITERION 
    TARGET test_cr_logger 
    DEPENDS 
        secure-logging 
        syslog-ng-secure-logging
        syslogformat
        cr_logger_lib
        cr_common_lib
        cr_verifier_lib
        cr_destination_lib
)

target_include_directories(test_cr_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..                       # For utils_slog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_destination
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_logger
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_common
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_verifier
)

add_unit_test(
    LIBTEST CRITERION 
    TARGET test_cr_verifier 
    DEPENDS 
        secure-logging 
        syslog-ng-secure-logging
        syslogformat
        cr_logger_lib
        cr_common_lib
        cr_verifier_lib
        cr_destination_lib
)

target_include_directories(test_cr_verifier PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..                       # For utils_slog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_destination
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_logger
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_common
    ${CMAKE_CURRENT_SOURCE_DIR}/../crashrecovery/cr_verifier
)

