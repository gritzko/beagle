cmake_minimum_required(VERSION 3.20)
project(git_test C)
set(CMAKE_C_STANDARD 23)
include_directories(.. ../..)

set(TEST_CXX_FLAGS "-g")
set(TEST_LDD_FLAGS "-g")

add_executable(GITtest GIT.c)
target_link_options(GITtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(GITtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(GITtest gitcompat abc-core)
add_test(NAME GITtest COMMAND GITtest)

add_executable(PKTtest PKT.c)
target_link_options(PKTtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(PKTtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(PKTtest gitcompat abc-core)
add_test(NAME PKTtest COMMAND PKTtest)

add_executable(GITPACKtest PACK.c)
target_link_options(GITPACKtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(GITPACKtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(GITPACKtest gitcompat abc-core)
add_test(NAME GITPACKtest COMMAND GITPACKtest)

add_executable(DELTtest DELT.c)
target_link_options(DELTtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(DELTtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(DELTtest gitcompat abc-core)
add_test(NAME DELTtest COMMAND DELTtest)

add_executable(DELTAROUNDtest DELTA_ROUND.c)
target_link_options(DELTAROUNDtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(DELTAROUNDtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(DELTAROUNDtest keeplib gitcompat dog abc-core)
add_test(NAME DELTAROUNDtest COMMAND DELTAROUNDtest)

if (WITH_SSH)
    add_executable(FETCHtest FETCH.c)
    target_link_options(FETCHtest PRIVATE ${TEST_LDD_FLAGS})
    target_compile_options(FETCHtest PRIVATE ${TEST_CXX_FLAGS})
    target_link_libraries(FETCHtest gitcompat abc-core)
    add_test(NAME FETCHtest COMMAND FETCHtest)

    add_executable(ROUNDtest ROUND.c)
    target_link_options(ROUNDtest PRIVATE ${TEST_LDD_FLAGS})
    target_compile_options(ROUNDtest PRIVATE ${TEST_CXX_FLAGS})
    target_link_libraries(ROUNDtest gitcompat abc-core)
    add_test(NAME ROUNDtest COMMAND ROUNDtest)

endif()

add_executable(WALKtest WALK.c)
target_link_options(WALKtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WALKtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WALKtest keeplib gitcompat dog abc-core)
add_test(NAME WALKtest COMMAND WALKtest)

add_executable(ZINFtest ZINF.c)
target_link_options(ZINFtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(ZINFtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(ZINFtest gitcompat abc-core)
add_test(NAME ZINFtest COMMAND ZINFtest)

add_executable(KEEPtest KEEP.c)
target_link_options(KEEPtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(KEEPtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(KEEPtest keeplib gitcompat dog abc-core)
add_test(NAME KEEPtest COMMAND KEEPtest)

add_executable(UNPKtest UNPK.c)
target_link_options(UNPKtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(UNPKtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(UNPKtest keeplib gitcompat dog abc-core)
add_test(NAME UNPKtest COMMAND UNPKtest)

add_executable(BOOKMARKtest BOOKMARK.c)
target_link_options(BOOKMARKtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(BOOKMARKtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(BOOKMARKtest keeplib gitcompat dog abc-core)
add_test(NAME BOOKMARKtest COMMAND BOOKMARKtest)

add_executable(PSTRtest PSTR.c)
target_link_options(PSTRtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(PSTRtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(PSTRtest keeplib gitcompat dog abc-core)
add_test(NAME PSTRtest COMMAND PSTRtest)

add_executable(REFStest REFS.c)
target_link_options(REFStest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(REFStest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(REFStest keeplib gitcompat dog abc-core)
add_test(NAME REFStest COMMAND REFStest)

add_executable(REFADVtest REFADV.c)
target_link_options(REFADVtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(REFADVtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(REFADVtest keeplib gitcompat dog abc-core)
add_test(NAME REFADVtest COMMAND REFADVtest)

add_executable(WIREtest WIRE.c)
target_link_options(WIREtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WIREtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WIREtest keeplib gitcompat dog abc-core)
add_test(NAME WIREtest COMMAND WIREtest)

#  upload-pack CLI verb (WIRE.md Phase 5).  The test spawns the built
#  `keeper` binary, so it depends on it being present in DOG_BIN_DIR.
add_executable(UPLOADPACKtest UPLOADPACK.c)
target_link_options(UPLOADPACKtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(UPLOADPACKtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(UPLOADPACKtest keeplib gitcompat dog abc-core)
add_dependencies(UPLOADPACKtest keeper-cli)
add_test(NAME UPLOADPACKtest COMMAND UPLOADPACKtest)
set_tests_properties(UPLOADPACKtest PROPERTIES
    ENVIRONMENT "KEEPER_BIN=${DOG_BIN_DIR}/keeper")

#  Client-side WIREFetch / WIREPush smoke (WIRE.md Phase 7).  Spawns
#  the built keeper binary as the local upload-pack / receive-pack
#  peer (file://… transport) and exercises end-to-end against a fresh
#  keeper repo.
add_executable(WIRECLIENTtest WIRE_CLIENT.c)
target_link_options(WIRECLIENTtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WIRECLIENTtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WIRECLIENTtest keeplib gitcompat dog abc-core)
add_dependencies(WIRECLIENTtest keeper-cli)
add_test(NAME WIRECLIENTtest COMMAND WIRECLIENTtest)
set_tests_properties(WIRECLIENTtest PROPERTIES
    ENVIRONMENT "KEEPER_BIN=${DOG_BIN_DIR}/keeper")

#  receive-pack CLI verb (WIRE.md Phase 6).  Same harness pattern as
#  UPLOADPACKtest: spawn the built keeper binary, drive its stdin/stdout.
add_executable(RECEIVEPACKtest RECEIVEPACK.c)
target_link_options(RECEIVEPACKtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(RECEIVEPACKtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(RECEIVEPACKtest keeplib gitcompat dog abc-core)
add_dependencies(RECEIVEPACKtest keeper-cli)
add_test(NAME RECEIVEPACKtest COMMAND RECEIVEPACKtest)
set_tests_properties(RECEIVEPACKtest PROPERTIES
    ENVIRONMENT "KEEPER_BIN=${DOG_BIN_DIR}/keeper")

