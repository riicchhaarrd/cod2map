# Changelog

## v1.0.0 — Initial Release

### Bug Fixes (original cod2map.exe bugs)

- **Non-deterministic output** — The original exe produces different .d3dbsp files every run because the triangle coalescing pass sorts surfaces by heap pointer address. Since Windows ASLR randomizes heap layout, the sort order changes between runs, causing different triangle batching, lightmap packing, and collision trees from the same .map file. Fixed by sorting on plane equation values as unsigned DWORDs instead of pointer addresses. Compile with `/DDETERMINISTIC_SORT` (enabled by default).

- **Missing material buffer overwrite** — When a material file is not found on disk, the original `LoadMaterial` writes into a shared default entry without proper isolation, corrupting the material table for subsequent lookups. This causes wrong surface flags, wrong lightmap assignment, and visual glitches on custom maps with missing materials. Fixed with clean `$default` fallback and failure tracking.

### New Features

- **Native x64 build** — 64-bit version runs approximately 2x faster than the original 32-bit binary on modern hardware.
