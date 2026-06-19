cmake_minimum_required(VERSION 3.20)
project(graf C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)


include_directories(.. .)

list(APPEND GRAF_HEADERS
    GRAF.h
    BLOB.h
    DAG.h
    JOIN.h
    REBASE.h
    WEAVE.h
)

list(APPEND GRAF_SOURCES
    GRAF.c
    GRAF.exe.c
    BLAME.c
    BLOB.c
    DAG.c
    DIFFREF.c
    GET.c
    INDEX.c
    JOIN.c
    LOG.c
    MAP.c
    MERGE.c
    REBASE.c
    WEAVE.c
)

add_library(graflib STATIC ${GRAF_HEADERS} ${GRAF_SOURCES})
target_compile_definitions(graflib PRIVATE NDEBUG)
target_link_libraries(graflib dog keeplib abc-core)

add_executable(graf-cli GRAF.cli.c)
target_compile_definitions(graf-cli PRIVATE NDEBUG)
target_link_libraries(graf-cli graflib dog keeplib abc-core)
add_dependencies(graf-cli bro-cli)
set_target_properties(graf-cli PROPERTIES OUTPUT_NAME graf RUNTIME_OUTPUT_DIRECTORY ${DOG_BIN_DIR})

add_subdirectory(test)

if (WITH_FUZZ)
    add_subdirectory(fuzz)
endif()
