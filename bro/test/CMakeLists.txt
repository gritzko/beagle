cmake_minimum_required(VERSION 3.20)
project(bro C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)


include_directories(.. ../..)

set(TEST_CXX_FLAGS "-g")
set(TEST_LDD_FLAGS "-g")

add_executable(NAVtest NAV.c)
target_link_options(NAVtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(NAVtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(NAVtest brolib dog abc-core)
add_test(NAME NAVtest COMMAND NAVtest)

add_executable(MAUStest MAUS.c)
target_link_options(MAUStest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(MAUStest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(MAUStest brolib dog abc-core)
add_test(NAME MAUStest COMMAND MAUStest)

add_executable(WRAPtest WRAP.c)
target_link_options(WRAPtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WRAPtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WRAPtest brolib dog abc-core)
add_test(NAME WRAPtest COMMAND WRAPtest)

# SEARCHtest includes ../BRO.c directly to reach file-static symbols
# (BROSearchNext, BROstate, bro_line_* encoding).  It must NOT link
# brolib — that would duplicate every BRO.c symbol — so it links the
# same downstream libs brolib pulls in (MEM-006 regression).
add_executable(SEARCHtest SEARCH.c ${CMAKE_CURRENT_SOURCE_DIR}/../MAUS.c)
target_link_options(SEARCHtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(SEARCHtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(SEARCHtest keeplib dog abc-core)
add_test(NAME SEARCHtest COMMAND SEARCHtest)

add_executable(EXECtest EXEC.c)
target_link_options(EXECtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(EXECtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(EXECtest brolib dog keeplib abc-core)
add_test(NAME EXECtest COMMAND EXECtest)
