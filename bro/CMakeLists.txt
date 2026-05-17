cmake_minimum_required(VERSION 3.20)
project(bro C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)


include_directories(.. .)

list(APPEND BRO_HEADERS
    BRO.h
    MAUS.h
)

list(APPEND BRO_SOURCES
    BRO.c
    BRO.exe.c
    MAUS.c
)

add_library(brolib STATIC ${BRO_HEADERS} ${BRO_SOURCES})
target_compile_definitions(brolib PRIVATE NDEBUG)
target_link_libraries(brolib keeplib dog abc-core)

add_executable(bro-cli BRO.cli.c)
target_compile_definitions(bro-cli PRIVATE NDEBUG)
target_link_libraries(bro-cli brolib keeplib dog abc-core)
set_target_properties(bro-cli PROPERTIES OUTPUT_NAME bro RUNTIME_OUTPUT_DIRECTORY ${DOG_BIN_DIR})

add_subdirectory(test)
