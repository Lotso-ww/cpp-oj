cmake_minimum_required(VERSION 3.10)
project(cpp-oj CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL mysqlclient REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${MYSQL_INCLUDE_DIRS}
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

add_executable(cpp-oj ${SOURCES})

target_link_libraries(cpp-oj
    ${MYSQL_LIBRARIES}
    httplib
    pthread
)

target_compile_options(cpp-oj PRIVATE -Wall -Wextra)