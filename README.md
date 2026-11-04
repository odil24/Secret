# Secret — Phase 02: Math Foundation

Initial C++23 scalar math layer for the Secret offline CPU renderer.

## Included
- Floating-point constants and scalar helpers.
- `Vector2<T>`, `Vector3<T>`, `Vector4<T>` with `float` and `double` aliases.
- Arithmetic, dot products, 3D cross product, Hadamard product, length, finite checks, and `try_normalize`.
- Dependency-free unit tests.

## CPU compatibility
The current math layer is scalar-only and does not require SSE4, AVX, or AVX2. Do not compile the entire target with `-march=native` or `/arch:AVX2`. SIMD kernels and runtime dispatch will be added separately after the scalar implementation is established as the reference path.

## Build and test (Embree not required)
```bash
cmake -S . -B build -DSECRET_WITH_EMBREE=OFF -DSECRET_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

MSVC uses dynamic CRT: `/MD` for non-Debug and `/MDd` for Debug. Windows 10 1507 remains a validation target; actual compatibility depends on the compiler toolset, CRT package, dependencies, and OS-level testing.

`try_normalize(out, min_length)` returns false for zero, too-small, or non-finite vectors and sets `out` to zero. Division operators require a non-zero scalar.

## Optional Embree integration
The Phase 01 RAII device wrapper is migrated to `secret::acceleration`. Enable it with `-DSECRET_WITH_EMBREE=ON` and pass the Embree 4.4.x package directory through `-Dembree_DIR=...`. Keep it OFF for standalone math tests.
