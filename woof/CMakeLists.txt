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
#  POL — the network-side modules abc-core excludes.  The dog libs
#  (graf/keeper/spot/sniff) provide the in-process projection dispatch
#  for `--api` mode; they were built against abc-core (a subset of abc),
#  so lazy static-archive linking resolves each symbol once.
target_link_libraries(wooflib dog graflib keeplib spotlib snifflib abc)

add_executable(woof-cli WOOF.cli.c)
target_link_libraries(woof-cli wooflib graflib keeplib spotlib snifflib dog abc)
set_target_properties(woof-cli PROPERTIES
    OUTPUT_NAME woof
    RUNTIME_OUTPUT_DIRECTORY ${DOG_BIN_DIR})

add_subdirectory(test)

if (WITH_FUZZ)
    add_subdirectory(fuzz)
endif()
