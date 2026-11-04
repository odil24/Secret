cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0091 NEW)

project(Secret VERSION 0.8.0 DESCRIPTION "Windows x64 CPU path tracer" LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Secret v0.8 supports Windows 10 x64 with MSVC only.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Secret v0.8 requires the x64 MSVC toolchain.")
endif()

option(SECRET_BUILD_TESTS "Build Secret unit tests" ON)
option(SECRET_BUILD_BENCHMARKS "Build and register CPU/BVH microbenchmarks" OFF)
option(SECRET_ENABLE_EMBREE "Build the optional Embree 4 device support library" OFF)

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

# Only AVX2 translation units are compiled with AVX2 enabled. The dispatcher,
# renderer and the rest of the executable remain baseline x64 for safe fallback.
set_source_files_properties(
    src/aabb_packet_avx2.cpp
    src/sphere_packet_avx2.cpp
    PROPERTIES COMPILE_OPTIONS "/arch:AVX2"
)

add_executable(secret
    src/main.cpp
    src/renderer.cpp
    ${SECRET_SIMD_SOURCES}
)
target_include_directories(secret PRIVATE include)
target_link_libraries(secret PRIVATE secret_options windowscodecs ole32)
target_compile_features(secret PRIVATE cxx_std_23)

if(SECRET_BUILD_TESTS OR SECRET_BUILD_BENCHMARKS)
    enable_testing()
endif()

if(SECRET_BUILD_TESTS)
    add_executable(secret_tests tests/tests.cpp ${SECRET_SIMD_SOURCES})
    target_include_directories(secret_tests PRIVATE include)
    target_link_libraries(secret_tests PRIVATE secret_options)
    target_compile_features(secret_tests PRIVATE cxx_std_23)
    add_test(NAME secret_tests COMMAND secret_tests)

    add_executable(secret_math_tests tests/math_tests.cpp)
    target_include_directories(secret_math_tests PRIVATE include)
    target_link_libraries(secret_math_tests PRIVATE secret_options)
    target_compile_features(secret_math_tests PRIVATE cxx_std_23)
    add_test(NAME secret_math_tests COMMAND secret_math_tests)

    set_tests_properties(secret_tests secret_math_tests PROPERTIES LABELS "unit")
endif()

if(SECRET_BUILD_BENCHMARKS)
    add_executable(secret_bvh_benchmark benchmarks/bvh_benchmark.cpp)
    target_include_directories(secret_bvh_benchmark PRIVATE include)
    target_link_libraries(secret_bvh_benchmark PRIVATE secret_options)
    target_compile_features(secret_bvh_benchmark PRIVATE cxx_std_23)
    add_test(NAME secret_bvh_benchmark COMMAND secret_bvh_benchmark)

    add_executable(secret_aabb_packet_benchmark
        benchmarks/aabb_packet_benchmark.cpp
        ${SECRET_SIMD_SOURCES}
    )
    target_include_directories(secret_aabb_packet_benchmark PRIVATE include)
    target_link_libraries(secret_aabb_packet_benchmark PRIVATE secret_options)
    target_compile_features(secret_aabb_packet_benchmark PRIVATE cxx_std_23)
    add_test(NAME secret_aabb_packet_benchmark COMMAND secret_aabb_packet_benchmark)

    add_executable(secret_sphere_packet_benchmark
        benchmarks/sphere_packet_benchmark.cpp
        ${SECRET_SIMD_SOURCES}
    )
    target_include_directories(secret_sphere_packet_benchmark PRIVATE include)
    target_link_libraries(secret_sphere_packet_benchmark PRIVATE secret_options)
    target_compile_features(secret_sphere_packet_benchmark PRIVATE cxx_std_23)
    add_test(NAME secret_sphere_packet_benchmark COMMAND secret_sphere_packet_benchmark)

    set_tests_properties(
        secret_bvh_benchmark
        secret_aabb_packet_benchmark
        secret_sphere_packet_benchmark
        PROPERTIES LABELS "benchmark" TIMEOUT 180
    )
endif()

if(SECRET_ENABLE_EMBREE)
    # Embree is deliberately opt-in: the native BVH remains the default backend.
    # Install the Embree 4 SDK and point CMAKE_PREFIX_PATH at its CMake package.
    find_package(embree 4 CONFIG REQUIRED)

    if(TARGET embree)
        set(SECRET_EMBREE_TARGET embree)
    elseif(TARGET embree::embree)
        set(SECRET_EMBREE_TARGET embree::embree)
    else()
        message(FATAL_ERROR "Embree 4 was found, but it did not export an embree target.")
    endif()

    add_library(secret_embree_support STATIC src/acceleration/embree_device.cpp)
    target_include_directories(secret_embree_support PUBLIC include)
    target_link_libraries(secret_embree_support PUBLIC secret_options ${SECRET_EMBREE_TARGET})
    target_compile_features(secret_embree_support PUBLIC cxx_std_23)
endif()
