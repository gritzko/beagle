cmake_minimum_required(VERSION 3.20)
project(mark C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories(.. ../..)

set(TEST_CXX_FLAGS "-g")
set(TEST_LDD_FLAGS "-g")

add_executable(MARKtest MARK.c)
target_link_options(MARKtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(MARKtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(MARKtest marklib dog abc-core)
add_test(NAME MARKtest COMMAND MARKtest)

#  CLI exit-code contract (DIS-013): --strict must fail a directory build
#  on any budget breach, not just a single file.  Driven against the
#  built `mark` binary since the gate is a process-level exit code.
add_test(NAME MARKstrictdir
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/strict-dir.sh $<TARGET_FILE:mark-cli>)
