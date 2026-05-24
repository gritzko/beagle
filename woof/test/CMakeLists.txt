cmake_minimum_required(VERSION 3.20)
project(woof C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories(.. ../..)

set(TEST_CXX_FLAGS "-g")
set(TEST_LDD_FLAGS "-g")

add_executable(WOOFHTTPtest HTTP.c)
target_link_options(WOOFHTTPtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WOOFHTTPtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WOOFHTTPtest wooflib dog abc)
add_test(NAME WOOFHTTPtest COMMAND WOOFHTTPtest)
