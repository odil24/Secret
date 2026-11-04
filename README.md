# Secret — v0.7

Windows 10 x64 CPU path tracer. Supported product toolchain: **Visual Studio 2022 / MSVC / C++23 / CMake 3.25+**. CMake intentionally rejects other operating systems and toolchains; portable core tests and microbenchmarks are run separately in the Linux development environment.

## v0.7 — packet BVH traversal and AVX2 sphere intersections

### Packet traversal

- Added a renderer-facing virtual `Hittable::hit_packet8` API with a correct scalar fallback for arbitrary primitives.
- BVH overrides the packet API and evaluates the AABB mask at every internal node, not only at the root.
- Per-lane closest-hit distances are propagated through leaf traversal.
- Leaf primitives receive packets directly. `Sphere` uses the AVX2 packet kernel; other primitive types use the scalar fallback.
- The packet traversal preserves nearest-hit semantics. Internal child order is fixed rather than sorted per lane; near-first scalar traversal remains available for single rays.

### AVX2 sphere kernel

- Added eight-wide ray/sphere intersection using SoA ray packets.
- Computes the quadratic discriminant and nearest valid root for each lane.
- Supports a separate `t_max` per lane and active-lane masks.
- Reconstructs hit points, normals and material references after the SIMD root calculation.
- Runtime dispatch checks AVX2 and OS YMM-state support. The AVX2 implementation is isolated in `src/sphere_packet_avx2.cpp`; only AVX2 translation units receive `/arch:AVX2`.
- Scalar packet fallback is retained for unsupported CPUs.

### Renderer integration

- Primary camera rays are generated and intersected in groups of eight.
- Each pixel retains its deterministic independent RNG stream.
- Secondary path bounces and Disney BSDF evaluation remain scalar in this version.
- Existing tile scheduler and thread pool are retained.

### Validation

- BVH packet traversal is compared against scalar BVH and brute-force results.
- Sphere packet roots and hit masks are compared against scalar sphere intersections.
- Tests include per-lane distance limits, inactive lanes, parallel AABB directions and randomized packets.
- AddressSanitizer and UndefinedBehaviorSanitizer tests pass in the Linux/GCC development environment.

## Material model

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

## Benchmarks

Enable optional benchmark targets with `-DSECRET_BUILD_BENCHMARKS=ON`.

The sphere packet microbenchmark compares scalar sphere intersections against the eight-lane AVX2 kernel over a fixed pre-generated set of 2,000,000 rays:

```powershell
cmake -S . -B build-bench -DSECRET_BUILD_BENCHMARKS=ON
cmake --build build-bench --config Release --target secret_sphere_packet_benchmark
.\build-bench\Release\secret_sphere_packet_benchmark.exe
```

The v0.7 Linux/GCC development measurement was approximately 2.8x faster at the median of five runs for the isolated sphere-intersection kernel (2,000,000 rays, identical hit count). This is a microbenchmark, not a full-frame rendering result; scene coherence, BVH traversal, BSDF cost and hardware affect end-to-end performance.

The v0.6 AABB packet benchmark reference was approximately 1.98x faster than scalar AABB testing. The v0.5 reference for 1,024 spheres and 30,000 rays was approximately 36.4x faster BVH traversal than linear traversal. These historical measurements use different microbenchmarks and should not be compared as end-to-end renderer speedups.

## Scope note

The v0.7 renderer batches primary-ray intersections and vectorizes sphere intersections and BVH node-bound tests. Secondary path bounces, Disney BSDF evaluation, and general primitive types remain scalar. Full SIMD path-state compaction and SIMD BSDF evaluation are later-stage work.
