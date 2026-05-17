# cod2map — Call of Duty 2 BSP Map Compiler

## What is this?

`cod2map.exe` is the map compiler that converts `.map` files (created in CoD2 Radiant) into `.d3dbsp` files (the binary BSP format the game engine loads). It handles BSP tree construction, portal visibility, lightmap allocation, collision mesh generation, shadow computation, and triangle processing.

## Requirements

- **Call of Duty 2** with mod tools installed (provides material definitions, IWD archives, and Radiant map files)

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Usage

```
cod2map -platform pc <mapname>
```

The compiler reads `.map` files from `map_source/` and outputs `.d3dbsp` and `.d3dprt` files to `maps/mp/` (or `maps/` for SP maps). Requires the CoD2 mod tools filesystem layout.

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
