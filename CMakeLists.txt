cmake_minimum_required(VERSION 3.10)
project(cpp-oj CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(MYSQL mysqlclient REQUIRED)
    pkg_check_modules(YAML yaml-cpp REQUIRED)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${MYSQL_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src/utils
)

set(SOURCES
    src/main.cc
    src/server/server.cc
    src/server/router.cc
    src/handler/problem_handler.cc
    src/handler/submit_handler.cc
    src/handler/auth_handler.cc
    src/handler/admin_handler.cc
    src/service/problem_service.cc
    src/service/executor_service.cc
    src/service/auth_service.cc
    src/model/problem.cc
    src/model/test_case.cc
    src/model/user.cc
    src/db/connection_pool.cc
    src/utils/config.cc
    src/utils/logger.cc
)

set(TEST_SOURCES
    src/utils/config.cc
)

set(LIBRARIES httplib pthread)
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
target_link_libraries(config_test /usr/lib/x86_64-linux-gnu/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest_main.a yaml-cpp pthread)
add_test(NAME config_test COMMAND config_test)