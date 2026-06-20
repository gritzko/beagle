cmake_minimum_required(VERSION 3.20)
project(mark C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories(.. .)

#  Ragel source is pre-generated and committed (MARKE.rl.c, the HTML escaper).
#  Regenerate with: ragel -C MARKE.c.rl -o MARKE.rl.c -L
#  Inline span decomposition moved into the StrictMark inline grammar
#  (dog/tok/MKDT.c.rl: the mkdtg machine / MKDTDecomposeSpan).
add_library(marklib STATIC
    MARK.h
    MARK.c
    MARKE.rl.c
)
target_link_libraries(marklib dog abc-core)

add_executable(mark-cli MARK.cli.c)
target_link_libraries(mark-cli marklib dog abc-core)
set_target_properties(mark-cli PROPERTIES
    OUTPUT_NAME mark
    RUNTIME_OUTPUT_DIRECTORY ${DOG_BIN_DIR})

add_subdirectory(test)

if (WITH_FUZZ)
add_subdirectory(fuzz)
endif()
