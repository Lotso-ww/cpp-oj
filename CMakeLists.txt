cmake_minimum_required(VERSION 3.10)
project(cpp-oj CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(MYSQL mysqlclient REQUIRED)
    pkg_check_modules(YAML yaml-cpp REQUIRED)
    pkg_check_modules(OPENSSL openssl QUIET)
endif()

find_package(OpenSSL REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/src/db
    ${CMAKE_SOURCE_DIR}/src/model
    ${CMAKE_SOURCE_DIR}/src/handler
    ${CMAKE_SOURCE_DIR}/src/service
    ${MYSQL_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    /usr/include/jsoncpp
    /usr/include/botan-2
)

set(SOURCES
    src/main.cc
    src/server/server.cc
    src/server/router.cc
    src/handler/problem_handler.cc
    src/handler/submit_handler.cc
    src/handler/auth_handler.cc
    src/handler/admin_handler.cc
    src/handler/run_handler.cc
    src/service/problem_service.cc
    src/service/executor_service.cc
    src/service/auth_service.cc
    src/service/session.cc
    src/service/session_manager.cc
    src/model/problem.cc
    src/model/test_case.cc
    src/model/user.cc
    src/db/connection_pool.cc
    src/utils/config.cc
    src/utils/logger.cc
    src/utils/password.cc
)

set(TEST_SOURCES
    src/utils/config.cc
    src/utils/logger.cc
    src/db/connection_pool.cc
    src/model/problem.cc
    src/model/test_case.cc
    src/model/user.cc
    src/utils/password.cc
)

set(LIBRARIES pthread yaml-cpp mysqlclient jsoncpp ssl crypto crypt botan-2)
if(MYSQL_LIBRARIES)
    list(APPEND LIBRARIES ${MYSQL_LIBRARIES})
endif()
if(YAML_LIBRARIES)
    list(APPEND LIBRARIES ${YAML_LIBRARIES})
endif()

add_executable(cpp-oj ${SOURCES})
target_link_libraries(cpp-oj ${LIBRARIES})
target_compile_options(cpp-oj PRIVATE -Wall -Wextra)

enable_testing()
add_executable(config_test ${TEST_SOURCES} tests/unit/config_test.cc)
target_link_libraries(config_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient crypt botan-2)
add_test(NAME config_test COMMAND config_test)

add_executable(logger_test ${TEST_SOURCES} tests/unit/logger_test.cc)
target_link_libraries(logger_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient crypt botan-2)
add_test(NAME logger_test COMMAND logger_test)

add_executable(connection_pool_test ${TEST_SOURCES} tests/unit/connection_pool_test.cc)
target_link_libraries(connection_pool_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient crypt botan-2)
add_test(NAME connection_pool_test COMMAND connection_pool_test)

add_executable(model_test ${TEST_SOURCES} tests/unit/problem_test.cc)
target_link_libraries(model_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient crypt botan-2)
add_test(NAME model_test COMMAND model_test)

add_executable(admin_handler_test ${TEST_SOURCES} src/handler/admin_handler.cc src/service/auth_service.cc src/service/session.cc src/service/session_manager.cc src/model/user.cc tests/unit/admin_handler_test.cc)
target_link_libraries(admin_handler_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient jsoncpp ssl crypto crypt botan-2)
add_test(NAME admin_handler_test COMMAND admin_handler_test)

add_executable(problem_handler_test ${TEST_SOURCES} src/handler/problem_handler.cc src/service/problem_service.cc tests/unit/problem_handler_test.cc)
target_link_libraries(problem_handler_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient jsoncpp crypt botan-2)
add_test(NAME problem_handler_test COMMAND problem_handler_test)

add_executable(problem_service_test ${TEST_SOURCES} src/service/problem_service.cc tests/unit/problem_service_test.cc)
target_link_libraries(problem_service_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient jsoncpp crypt botan-2)
add_test(NAME problem_service_test COMMAND problem_service_test)

add_executable(executor_test ${TEST_SOURCES} src/service/executor_service.cc tests/unit/executor_test.cc)
target_link_libraries(executor_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient crypt botan-2)
add_test(NAME executor_test COMMAND executor_test)

add_executable(run_handler_test ${TEST_SOURCES} src/handler/run_handler.cc src/service/executor_service.cc tests/unit/run_handler_test.cc)
target_link_libraries(run_handler_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient jsoncpp crypt botan-2)
add_test(NAME run_handler_test COMMAND run_handler_test)

add_executable(session_test ${TEST_SOURCES} src/service/session.cc src/service/session_manager.cc tests/unit/session_test.cc)
target_link_libraries(session_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient crypt botan-2)
add_test(NAME session_test COMMAND session_test)

add_executable(auth_service_test ${TEST_SOURCES} src/service/session.cc src/service/session_manager.cc src/service/auth_service.cc src/model/user.cc tests/unit/auth_service_test.cc)
target_link_libraries(auth_service_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient jsoncpp crypt botan-2)
add_test(NAME auth_service_test COMMAND auth_service_test)

add_executable(auth_handler_test ${TEST_SOURCES} src/handler/auth_handler.cc src/service/auth_service.cc src/service/session.cc src/service/session_manager.cc src/model/user.cc tests/unit/auth_handler_test.cc)
target_link_libraries(auth_handler_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient jsoncpp crypt botan-2)
add_test(NAME auth_handler_test COMMAND auth_handler_test)

add_executable(password_test ${TEST_SOURCES} tests/unit/password_test.cc)
target_link_libraries(password_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread mysqlclient crypt botan-2)
add_test(NAME password_test COMMAND password_test)

add_executable(create_admin tools/create_admin.cc
    src/db/connection_pool.cc
    src/utils/config.cc
    src/utils/logger.cc
    src/model/user.cc
    src/utils/password.cc)
target_link_libraries(create_admin yaml-cpp mysqlclient jsoncpp ssl crypto crypt botan-2 pthread)

add_executable(reset_db tools/reset_db.cc)
target_link_libraries(reset_db mysqlclient crypt botan-2 pthread)
