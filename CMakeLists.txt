cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0091 NEW)

project(Secret VERSION 0.7.0 DESCRIPTION "Windows x64 CPU path tracer" LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Secret v0.7 supports Windows 10 x64 with MSVC only.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Secret v0.7 requires the x64 MSVC toolchain.")
endif()

option(SECRET_BUILD_TESTS "Build Secret unit tests" ON)
option(SECRET_BUILD_BENCHMARKS "Build CPU/BVH microbenchmarks" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

add_library(secret_options INTERFACE)
target_compile_options(secret_options INTERFACE
    /W4
    /permissive-
    /Zc:__cplusplus
    /EHsc
    /utf-8
)
target_compile_definitions(secret_options INTERFACE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

set(SECRET_SIMD_SOURCES
    src/aabb_packet.cpp
    src/aabb_packet_avx2.cpp
    src/sphere_packet.cpp
    src/sphere_packet_avx2.cpp
)

# Only the AVX2 translation unit is compiled with AVX2 enabled. The dispatcher,
# renderer and the rest of the executable remain baseline x64 for safe fallback.
if(MSVC)
    set_source_files_properties(src/aabb_packet_avx2.cpp src/sphere_packet_avx2.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
else()
    set_source_files_properties(src/aabb_packet_avx2.cpp src/sphere_packet_avx2.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
endif()

add_executable(secret
    src/main.cpp
    src/renderer.cpp
    ${SECRET_SIMD_SOURCES}
)
target_include_directories(secret PRIVATE include)
target_link_libraries(secret PRIVATE secret_options windowscodecs ole32)
target_compile_features(secret PRIVATE cxx_std_23)

if(SECRET_BUILD_TESTS)
    enable_testing()
    add_executable(secret_tests tests/tests.cpp ${SECRET_SIMD_SOURCES})
    target_include_directories(secret_tests PRIVATE include)
    target_link_libraries(secret_tests PRIVATE secret_options)
    target_compile_features(secret_tests PRIVATE cxx_std_23)
    add_test(NAME secret_tests COMMAND secret_tests)
endif()

if(SECRET_BUILD_BENCHMARKS)
    add_executable(secret_bvh_benchmark benchmarks/bvh_benchmark.cpp)
    target_include_directories(secret_bvh_benchmark PRIVATE include)
    target_link_libraries(secret_bvh_benchmark PRIVATE secret_options)
    target_compile_features(secret_bvh_benchmark PRIVATE cxx_std_23)

    add_executable(secret_aabb_packet_benchmark
        benchmarks/aabb_packet_benchmark.cpp
        ${SECRET_SIMD_SOURCES}
    )
    target_include_directories(secret_aabb_packet_benchmark PRIVATE include)
    target_link_libraries(secret_aabb_packet_benchmark PRIVATE secret_options)
    target_compile_features(secret_aabb_packet_benchmark PRIVATE cxx_std_23)

    add_executable(secret_sphere_packet_benchmark
        benchmarks/sphere_packet_benchmark.cpp
        ${SECRET_SIMD_SOURCES}
    )
    target_include_directories(secret_sphere_packet_benchmark PRIVATE include)
    target_link_libraries(secret_sphere_packet_benchmark PRIVATE secret_options)
    target_compile_features(secret_sphere_packet_benchmark PRIVATE cxx_std_23)
endif()
