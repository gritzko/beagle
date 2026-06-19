cmake_minimum_required(VERSION 3.20)
project(dog C)

set(CMAKE_C_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories(../..)
include_directories(..)
include_directories(../tok ../git)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUZZ_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FUZZ_C_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FUZZ_LDD_FLAGS}")

add_executable(TOKfuzz TOK.c)
target_link_libraries(TOKfuzz dog abc-core)

add_executable(TOK2fuzz TOK2.c)
target_link_libraries(TOK2fuzz dog abc-core)

add_executable(PACKRESOLVEfuzz PACKRESOLVE.c)
target_link_libraries(PACKRESOLVEfuzz dog abc-core)
