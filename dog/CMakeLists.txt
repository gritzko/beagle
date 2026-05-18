cmake_minimum_required(VERSION 3.20)
project(libdog C)

set(CMAKE_C_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

find_package(ZLIB REQUIRED)

include_directories(.. . tok git)

set(TEST_CXX_FLAGS "-g")
set(TEST_LDD_FLAGS "-g")

list(APPEND TOK_HEADERS
    tok/TOK.h
    tok/CT.h
    tok/GOT.h
    tok/PYT.h
    tok/JST.h
    tok/RST.h
    tok/JAT.h
    tok/CPPT.h
    tok/CST.h
    tok/HTMT.h
    tok/CSST.h
    tok/JSONT.h
    tok/SHT.h
    tok/RBT.h
    tok/HST.h
    tok/MLT.h
    tok/JLT.h
    tok/PHPT.h
    tok/AGDT.h
    tok/VERT.h
    tok/TST.h
    tok/KTT.h
    tok/SCLT.h
    tok/SWFT.h
    tok/DARTT.h
    tok/ZIGT.h
    tok/DT.h
    tok/LUAT.h
    tok/PRLT.h
    tok/RT.h
    tok/ELXT.h
    tok/ERLT.h
    tok/NIMT.h
    tok/NIXT.h
    tok/VIMT.h
    tok/YMLT.h
    tok/TOMLT.h
    tok/SQLT.h
    tok/GQLT.h
    tok/PRTT.h
    tok/HCLT.h
    tok/SCSST.h
    tok/LAXT.h
    tok/CLJT.h
    tok/CMKT.h
    tok/DKFT.h
    tok/FORT.h
    tok/FSHT.h
    tok/GLMT.h
    tok/GLST.h
    tok/MAKT.h
    tok/ODNT.h
    tok/PWST.h
    tok/SOLT.h
    tok/TYST.h
    tok/TXTT.h
    tok/MDT.h
    tok/MKDT.h
    tok/LLT.h
)

list(APPEND TOK_SOURCES
    tok/TOK.c
    tok/CT.c
    tok/CT.rl.c
    tok/GOT.c
    tok/GOT.rl.c
    tok/PYT.c
    tok/PYT.rl.c
    tok/JST.c
    tok/JST.rl.c
    tok/RST.c
    tok/RST.rl.c
    tok/JAT.c
    tok/JAT.rl.c
    tok/CPPT.c
    tok/CPPT.rl.c
    tok/CST.c
    tok/CST.rl.c
    tok/HTMT.c
    tok/HTMT.rl.c
    tok/CSST.c
    tok/CSST.rl.c
    tok/JSONT.c
    tok/JSONT.rl.c
    tok/SHT.c
    tok/SHT.rl.c
    tok/RBT.c
    tok/RBT.rl.c
    tok/HST.c
    tok/HST.rl.c
    tok/MLT.c
    tok/MLT.rl.c
    tok/JLT.c
    tok/JLT.rl.c
    tok/PHPT.c
    tok/PHPT.rl.c
    tok/AGDT.c
    tok/AGDT.rl.c
    tok/VERT.c
    tok/VERT.rl.c
    tok/TST.c
    tok/TST.rl.c
    tok/KTT.c
    tok/KTT.rl.c
    tok/SCLT.c
    tok/SCLT.rl.c
    tok/SWFT.c
    tok/SWFT.rl.c
    tok/DARTT.c
    tok/DARTT.rl.c
    tok/ZIGT.c
    tok/ZIGT.rl.c
    tok/DT.c
    tok/DT.rl.c
    tok/LUAT.c
    tok/LUAT.rl.c
    tok/PRLT.c
    tok/PRLT.rl.c
    tok/RT.c
    tok/RT.rl.c
    tok/ELXT.c
    tok/ELXT.rl.c
    tok/ERLT.c
    tok/ERLT.rl.c
    tok/NIMT.c
    tok/NIMT.rl.c
    tok/NIXT.c
    tok/NIXT.rl.c
    tok/VIMT.c
    tok/VIMT.rl.c
    tok/YMLT.c
    tok/YMLT.rl.c
    tok/TOMLT.c
    tok/TOMLT.rl.c
    tok/SQLT.c
    tok/SQLT.rl.c
    tok/GQLT.c
    tok/GQLT.rl.c
    tok/PRTT.c
    tok/PRTT.rl.c
    tok/HCLT.c
    tok/HCLT.rl.c
    tok/SCSST.c
    tok/SCSST.rl.c
    tok/LAXT.c
    tok/LAXT.rl.c
    tok/CLJT.c
    tok/CLJT.rl.c
    tok/CMKT.c
    tok/CMKT.rl.c
    tok/DKFT.c
    tok/DKFT.rl.c
    tok/FORT.c
    tok/FORT.rl.c
    tok/FSHT.c
    tok/FSHT.rl.c
    tok/GLMT.c
    tok/GLMT.rl.c
    tok/GLST.c
    tok/GLST.rl.c
    tok/MAKT.c
    tok/MAKT.rl.c
    tok/ODNT.c
    tok/ODNT.rl.c
    tok/PWST.c
    tok/PWST.rl.c
    tok/SOLT.c
    tok/SOLT.rl.c
    tok/TYST.c
    tok/TYST.rl.c
    tok/TXTT.rl.c
    tok/MDT.c
    tok/MDT.rl.c
    tok/MKDT.c
    tok/MKDT.rl.c
    tok/LLT.c
    tok/LLT.rl.c
)

list(APPEND TOK_HEADERS tok/KEYW.h)
list(APPEND TOK_SOURCES tok/KEYW.c)

list(APPEND TOK_HEADERS tok/BRCT.h)
list(APPEND TOK_SOURCES tok/BRCT.c)

list(APPEND TOK_HEADERS tok/DEF.h)
list(APPEND TOK_SOURCES tok/DEF.c)

# git-format primitives (was: in keeper/)
list(APPEND TOK_HEADERS
    git/SHA1.h
    git/IGNO.h
    git/GIT.h
    git/PKT.h
    git/PACK.h
    git/DELT.h
    git/ZINF.h
    git/CFG.h
)
list(APPEND TOK_SOURCES
    git/IGNO.c
    git/GIT.c
    git/PKT.c
    git/PACK.c
    git/DELT.c
    git/ZINF.c
    git/CFG.c
    git/CFG.rl.c
)

list(APPEND TOK_HEADERS HUNK.h)
list(APPEND TOK_SOURCES HUNK.c)

list(APPEND TOK_HEADERS HOME.h)
list(APPEND TOK_SOURCES HOME.c)

list(APPEND TOK_HEADERS FRAG.h)
list(APPEND TOK_SOURCES FRAG.rl.c)

list(APPEND TOK_HEADERS QURY.h)
list(APPEND TOK_SOURCES QURY.rl.c)

list(APPEND TOK_HEADERS DPATH.h)
list(APPEND TOK_SOURCES DPATH.rl.c)

list(APPEND TOK_HEADERS ULOG.h)
list(APPEND TOK_SOURCES ULOG.c)

list(APPEND TOK_HEADERS DOG.h)
list(APPEND TOK_SOURCES DOG.c)

list(APPEND TOK_HEADERS CLI.h)
list(APPEND TOK_SOURCES CLI.c)

add_library(dog STATIC ${TOK_HEADERS} ${TOK_SOURCES}
            sha1dc/sha1.c sha1dc/ubc_check.c)
target_link_libraries(dog abc-core ZLIB::ZLIB)

add_executable(dogtok tok/TOK.cli.c)
target_link_libraries(dogtok dog abc-core)

add_subdirectory(test)

if (WITH_FUZZ)
add_subdirectory(fuzz)
endif()
