cmake_minimum_required(VERSION 3.20)
project(woof C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories(.. .)

add_library(wooflib STATIC
    WOOF.h
    WOOF.c
    ROUTE.c
    CONN.c
)
#  Full `abc` (not abc-core) because woof pulls in HTTP / NET / TCP /
#  POL — the network-side modules abc-core excludes.
target_link_libraries(wooflib dog abc)

add_executable(woof-cli WOOF.cli.c)
target_link_libraries(woof-cli wooflib dog abc)
set_target_properties(woof-cli PROPERTIES
    OUTPUT_NAME woof
    RUNTIME_OUTPUT_DIRECTORY ${DOG_BIN_DIR})

add_subdirectory(test)
