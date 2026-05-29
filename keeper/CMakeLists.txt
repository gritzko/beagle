cmake_minimum_required(VERSION 3.20)
project(keeper C)
set(CMAKE_C_STANDARD 23)
include_directories(.. .)

find_package(ZLIB REQUIRED)

add_library(keeplib STATIC KEEP.c KEEP.exe.c REFS.c REFADV.c RESOLVE.c WALK.c UNPK.c PSTR.c WIRE.c WIRE.rl.c WIRECLI.c RECV.c PROJ.c SUBS.c)
target_link_libraries(keeplib dog abc-core ZLIB::ZLIB)

#  keeper-cli no longer pushes to graf/spot — under the new
#  arrangement (dog/DOG.md §10a) those dogs reindex themselves when
#  `be get URI` spawns them.  keeplib stays the only direct dep.
add_executable(keeper-cli KEEP.cli.c)
target_link_libraries(keeper-cli keeplib dog abc-core)
set_target_properties(keeper-cli PROPERTIES OUTPUT_NAME keeper RUNTIME_OUTPUT_DIRECTORY ${DOG_BIN_DIR})

add_executable(git-dl git-dl.cli.c)
target_link_libraries(git-dl keeplib dog abc-core)

add_subdirectory(test)
