cmake_minimum_required(VERSION 3.20)
project(beagle C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)


include_directories(.. .)

add_executable(be-cli BE.cli.c DISPATCH.c SUBS.c)
target_compile_definitions(be-cli PRIVATE NDEBUG)
target_link_libraries(be-cli snifflib graflib spotlib keeplib dog abc-core)
set_target_properties(be-cli PROPERTIES OUTPUT_NAME be RUNTIME_OUTPUT_DIRECTORY ${DOG_BIN_DIR})

#  GET-028: `be` execs these dog binaries at runtime; without an explicit dep
#  `ninja be` rebuilds keeplib objects but never relinks them, shipping a stale
#  helper (e.g. an old `keeper` whose ingest fails an ssh clone with ZINFFAIL).
add_dependencies(be-cli keeper-cli spot-cli graf-cli sniff-cli)

add_subdirectory(test)
