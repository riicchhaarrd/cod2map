# cod2map — Call of Duty 2 BSP Map Compiler (Reconstructed Source)

Fully reconstructed C source code for `cod2map.exe`, the BSP map compiler shipped with Call of Duty 2's mod tools. Reverse-engineered from the original 2005 binary to produce **bit-identical** `.d3dbsp` output.

## What is this?

`cod2map.exe` is the map compiler that converts `.map` files (created in CoD2 Radiant) into `.d3dbsp` files (the binary BSP format the game engine loads). It handles BSP tree construction, portal visibility, lightmap allocation, collision mesh generation, shadow computation, and triangle processing.

This project reconstructs the complete source code from the original binary, cleaned up to readable C in the style of id Software's q3map2 (the open-source tool cod2map was originally based on).

## Features

- **Bit-identical output** — produces byte-for-byte identical `.d3dbsp` and `.d3dprt` files as the original `cod2map.exe` across all 15 MP and 39 SP maps
- **x86 and x64 builds** — native 64-bit build runs ~2x faster than the original 32-bit binary
- **Clean C source** — fully commented, named constants, proper struct definitions, q3map2-style formatting
- **All original functionality preserved** — brush CSG, patch/terrain surfaces, BSP tree, portals, visibility, lightmaps, collision, shadows, T-junction fixing, triangle optimization

## Requirements

- **Windows 10/11** (Windows-only; Linux/Mac would require porting the platform layer)
- **Visual Studio 2022 or later** (MSVC compiler required for x87 FPU precision matching)
- **Call of Duty 2** with mod tools installed (provides material definitions, IWD archives, and Radiant map files)

## Building

Edit `build.bat` and set `SRCDIR` to your source directory and `OUTDIR` to your CoD2 bin directory, then run:

```bat
build.bat
```

This builds both x86 (`cod2map.exe`) and x64 (`cod2map64.exe`) and runs a comparison test against the original binary.

## Usage

```
cod2map -platform pc <mapname>
```

The compiler reads `.map` files from `map_source/` and outputs `.d3dbsp` and `.d3dprt` files to `maps/mp/` (or `maps/` for SP maps). Requires the CoD2 mod tools filesystem layout.

## Project Structure

```
cod2src/
  cod2map.h              Master header (structs, defines, prototypes)
  bsp.c                  Main BSP compilation entry point
  brush.c                Brush creation, beveling, splitting
  collision.c            Collision mesh generation (AABB trees, edges, partitions)
  map.c                  .map file parser (entities, brushes, patches, prefabs)
  portals.c              Portal generation and visibility
  lightmaps.c            Lightmap UV computation and allocation
  shadows.c              Shadow volume computation
  shadows_midpoint.c     Midpoint shadow sampling
  surface.c              Surface processing and BSP filtering
  tris.c                 Triangle processing pipeline
  tris_coalesce.c        Winding coalescing
  tris_gridtree.c        Spatial grid acceleration
  tris_mergeconcave.c    Concave winding merging
  tris_mergeverts.c      Vertex merging
  tris_tesselate.c       Winding tessellation
  tris_tjunc.c           T-junction fixing
  vis.c                  PVS visibility computation
  mesh.c                 Curve/patch mesh subdivision
  patch.c                Patch surface processing
  tree.c                 BSP tree utilities
  facebsp.c              Face BSP construction
  leakfile.c             Leak detection and .lin file output
  materials.c            Material/shader loading
  writebsp.c             BSP file writing and byte-swapping
  win_shared.c           Windows platform layer
  common/
    bspfile.c            BSP lump I/O and byte-swap
    polylib.c            Winding polygon library
    targetplatform.c     Platform configuration
    texturevecs.c        Texture vector computation
  universal/
    assertive.c          Assert system and crash handler
    com_files.c          Filesystem (IWD/pk3 archives, search paths)
    com_math.c           Math library (vectors, matrices, quaternions)
    com_memory.c         Memory management (hunk allocator, Z_Malloc)
    com_shared.c         Shared utilities (printf, error handling)
    dvar.c               Dynamic variable system
    q_parse.c            Token parser
    q_shared.c           Shared quake utilities (string ops, byte-swap)
    tangentspace.c       Tangent space generation
  libs/
    cmdlib.c             Command-line utilities
    zlib/                zlib compression (for IWD/pk3 reading)
    minizip/             minizip unzip library
```

## Legal Notice

This project is a **reverse-engineered reconstruction** of a binary tool (`cod2map.exe`) distributed as part of the Call of Duty 2 Mod Tools by Infinity Ward / Activision.

**This repository does not contain:**
- Any game assets (textures, models, sounds, maps, scripts)
- Any copyrighted game content
- The original `cod2map.exe` binary
- Any content from the Call of Duty 2 game itself

**Legal basis:**
- Reverse engineering for **interoperability** is protected under DMCA 17 U.S.C. Section 1201(f), EU Directive 2009/24/EC Article 6, and equivalent laws in most jurisdictions
- This tool exists solely to enable creation of custom content (maps) for Call of Duty 2
- A legally obtained copy of Call of Duty 2 with mod tools is required to use this project
- No copy protection or DRM was circumvented -- `cod2map.exe` is an unprotected development tool distributed freely with the mod tools
- This project is provided for **educational, research, and modding purposes**

This is a community preservation effort for a 2005-era modding tool. The original mod tools are no longer officially distributed or supported. This reconstruction ensures the CoD2 mapping community can continue to create content, fix bugs in the original tool, and build 64-bit versions for modern systems.

**This project is NOT comparable to game client/server reverse engineering projects.** It does not enable playing Call of Duty 2 without purchasing it, does not provide multiplayer services, does not bypass any authentication or copy protection, and does not compete with any current Activision product. It is solely a reconstruction of an unsupported, freely-distributed development utility for creating user-generated content.

**If you are a rights holder and have concerns, please open an issue before filing any takedown request.**

## Credits

- **Reconstructed by Rose** -- full reverse engineering, source reconstruction, and cleanup
- **Original tool by Infinity Ward** -- based on id Software's q3map2
- **id Software** -- q3map2 (GPL licensed), the foundation this tool was built on

## License

This reconstructed source code is provided as-is for educational and interoperability purposes. The original q3map2 code that cod2map was derived from is licensed under the GNU General Public License v2. To the extent that this reconstruction constitutes a derivative work of GPL-licensed code, it is made available under the same terms.
