# Credits and third-party notices

## Disney principled BRDF

The Disney BRDF implementation in `include/secret/render/material.hpp` is an independent CPU adaptation of the published equations and sampling approach used in:

- Shih-Chin Weng, `rlShaders/src/rlDisney.cpp` — https://github.com/shihchinw/rlShaders/blob/master/src/rlDisney.cpp
- Brent Burley, *Physically Based Shading at Disney*, SIGGRAPH 2012 course notes.
- Bruce Walter et al., *Microfacet Models for Refraction through Rough Surfaces*, EGSR 2007.
- Eric Heitz and Eugene d'Eon, *Importance Sampling Microfacet-Based BSDFs using the Distribution of Visible Normals*, EGSR 2014.

The referenced `rlShaders` repository declares the MIT License. Its notice is reproduced in `third_party/rlShaders_LICENSE.txt`.

## LTC multiple-scattering sheen

Secret v0.4 integrates the LTC sheen lobe into the same Disney BSDF. Its coefficient fit is the 32x32 **SGGX-volume** table published with:

- Tizian Zeltner, Brent Burley, and Matt Jen-Yuan Chiang, *Practical Multiple-Scattering Sheen Using Linearly Transformed Cosines*, SIGGRAPH 2022.
- Official reference implementation: https://github.com/tizian/ltc-sheen
- Reference PBRT implementation: `pbrt-v3/src/materials/sheenltc.cpp`.
- Original coefficient table: `fitting/python/data/ltc_table_sheen_volume.cpp`.

The original table is distributed under Apache License 2.0. Secret stores a quantized signed-16-bit representation and reconstructs the coefficients before bilinear interpolation. The full license and notices are in `third_party/Apache-2.0-LICENSE.txt` and `third_party/NOTICE.txt`.

The user-provided integration reference is Borja García Quiroga's https://github.com/borjagq/LTC-based-sheen-models. It describes the Zeltner LTC sheen approach in an OpenGL implementation. No files from that repository are copied verbatim; its MIT notice is included in `third_party/BorjaGq-MIT-LICENSE.txt` and attribution is listed in `third_party/NOTICE.txt`.

The Secret port does not copy the PBRT or OpenGL renderer framework. It uses the published LTC transform, sampling/PDF relationships, and fitted coefficient data in a CPU path-tracing BSDF.
