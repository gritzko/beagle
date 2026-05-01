cmake_minimum_required(VERSION 3.20)
project(graf C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories(../..)
include_directories(..)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUZZ_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FUZZ_C_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FUZZ_LDD_FLAGS}")

add_executable(JOINfuzz JOIN.c)
target_link_libraries(JOINfuzz graflib dog abc-core)

add_executable(NEILfuzz NEIL.c)
target_link_libraries(NEILfuzz graflib dog abc-core)

add_executable(WEAVEfuzz WEAVE.c)
target_link_libraries(WEAVEfuzz graflib dog abc-core)
