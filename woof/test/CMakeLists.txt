cmake_minimum_required(VERSION 3.20)
project(woof C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories(.. ../..)

set(TEST_CXX_FLAGS "-g")
set(TEST_LDD_FLAGS "-g")

add_executable(WOOFHTTPtest HTTP.c)
target_link_options(WOOFHTTPtest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WOOFHTTPtest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WOOFHTTPtest wooflib dog abc)
add_test(NAME WOOFHTTPtest COMMAND WOOFHTTPtest)

#  MEM-030: WOOFServeStatic must not leak the asset mmap (VMA + fd) when
#  the conn out-ring is too small for envelope+body (BNOROOM path).
add_executable(WOOFStatictest STATIC.c)
target_link_options(WOOFStatictest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WOOFStatictest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WOOFStatictest wooflib dog abc)
add_test(NAME WOOFStatictest COMMAND WOOFStatictest)

#  MEM-038: WOOFRenderHunks must not leak BASS arena — POL drives the
#  pipe_cb / serve_inproc callbacks with no call()/try() frame, so the
#  per-hunk TOK carve must be rewound by the render path itself.
add_executable(WOOFHunkLeaktest HUNKLEAK.c)
target_link_options(WOOFHunkLeaktest PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WOOFHunkLeaktest PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WOOFHunkLeaktest wooflib dog abc)
add_test(NAME WOOFHunkLeaktest COMMAND WOOFHunkLeaktest)
