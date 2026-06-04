cmake_minimum_required(VERSION 3.20)
project(woof_fuzz C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories(../..)
include_directories(..)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUZZ_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FUZZ_C_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FUZZ_LDD_FLAGS}")

#  graflib (+keeplib transitively) gives the in-process projection
#  dispatch: GRAFExec is called as a library, not forked.  woof needs
#  the full `abc` (HTTP/NET/TCP/POL); the dog libs were built against
#  `abc-core` (a strict subset), so lazy static-archive linking resolves
#  each symbol once — network objects only exist in `abc`.
add_executable(WOOFURIfuzz URI.c)
target_link_libraries(WOOFURIfuzz wooflib graflib keeplib dog abc)
