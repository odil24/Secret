# Secret — v0.8

Windows 10 x64 CPU path tracer. Supported product toolchain: **Visual Studio 2022 / MSVC / C++23 / CMake 3.25+**. CMake intentionally rejects other operating systems and toolchains.

## v0.8 — math tests, optional Embree support and benchmark CI

- Added the missing `secret::math` API in `include/secret/math/Math.hpp`: scalar helpers/constants and `Vector2f`, `Vector3f`, `Vector4f`.
- Wired `tests/math_tests.cpp` into CMake and CTest as `secret_math_tests`.
- Completed the Embree 4 device RAII wrapper with its public header and an opt-in CMake target. Embree is not required to build or run Secret; the renderer continues to use its native BVH.
- Registered the BVH, AABB packet and sphere packet microbenchmarks as CTest tests. Windows CI now builds and executes all three and fails if their reference/SIMD results disagree.
- Bumped the project version to 0.8.0.

## Renderer and material model

There is exactly one implemented surface material: `DisneyMaterial`. There are no separate Lambertian, Metal, or Dielectric material classes.

- Disney Burley diffuse and Hanrahan–Krueger subsurface approximation.
- Anisotropic GGX/Trowbridge–Reitz, Smith masking-shadowing, Schlick Fresnel, and Heitz visible-normal sampling.
- LTC multiple-scattering Sheen based on the Zeltner et al. 2022 SGGX volume fit, with `sheen_roughness`.
- Opacity / stochastic coverage.
- Diffuse, GGX and LTC Sheen sampling use a matching mixture PDF.

The LTC source fit and third-party licenses/credits are retained in `include/secret/render/detail/ltc_sheen_volume_table.hpp`, `CREDITS.md`, and `third_party/`.

## PNG output

Rendered images are saved as PNG through Windows Imaging Component (WIC). Linear HDR radiance is Reinhard tone-mapped and encoded with the sRGB transfer function. Default output: `render.png`.

## Requirements

- Windows 10 x64
- Visual Studio 2022 with Desktop development with C++ workload
- MSVC toolset with C++23 support
- CMake 3.25 or newer

## Build with MSVC

Run from Developer PowerShell for VS 2022:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Release build:

```powershell
cmake --build --preset windows-msvc-release
```

## Run

```powershell
.\build\windows-msvc-debug\Debug\secret.exe
.\build\windows-msvc-debug\Debug\secret.exe --width 1280 --height 720 --spp 64 --depth 12 --tile 16 --output render.png
```

## CPU feature detection

The executable checks CPU AVX/AVX2 support and OS YMM-state support. AVX2 kernels are isolated in dedicated translation units; the executable is not globally compiled with `/arch:AVX2`. CPUs without AVX2 use scalar implementations.

## Benchmarks and CI

Build all three benchmarks:

```powershell
cmake -S . -B build-bench -G "Visual Studio 17 2022" -A x64 `
  -DSECRET_BUILD_TESTS=ON -DSECRET_BUILD_BENCHMARKS=ON
cmake --build build-bench --config Release --parallel
```

Run unit tests and benchmarks separately:

```powershell
ctest --test-dir build-bench -C Release -L unit --output-on-failure
ctest --test-dir build-bench -C Release -L benchmark --output-on-failure
```

The benchmark executables return a failure code if their hit counts/checksums differ from the scalar/reference implementation. Timing values are informational and are not used as CI pass/fail thresholds because hosted-runner performance is variable.

## Optional Embree 4 device support

Secret's own BVH remains the default and only renderer-integrated acceleration structure. The optional Embree target currently provides a correctly owned Embree 4 device for future backend work; it does not silently replace the native BVH or claim scene traversal integration.

Install the Embree 4 SDK, make its CMake package discoverable (for example by setting `CMAKE_PREFIX_PATH`), then configure:

```powershell
cmake -S . -B build-embree -G "Visual Studio 17 2022" -A x64 `
  -DSECRET_ENABLE_EMBREE=ON -DSECRET_BUILD_TESTS=ON
cmake --build build-embree --config Release --parallel
```

With `SECRET_ENABLE_EMBREE=OFF` (the default), no Embree headers, libraries, or SDK installation are required.

## Scope note

The v0.8 renderer batches primary-ray intersections and vectorizes sphere intersections and BVH node-bound tests. Secondary path bounces, Disney BSDF evaluation, and general primitive types remain scalar. Full SIMD path-state compaction and SIMD BSDF evaluation are later-stage work.
