# Codebase Audit: Native Hill Climb Racing Port for PSP / PC

**Date:** November 2026  
**Status:** Milestone M0 Baseline Audit  
**Target Platforms:** Sony PlayStation Portable (PSP-1000/2000/3000, 333 MHz Allegrex MIPS) & PC (Linux/Windows SDL2 + OpenGL)

---

## 1. Executive Summary

A comprehensive code audit of the existing codebase in `c:\Projects\hcr-psp` has been conducted against the fidelity requirements of the original Hill Climb Racing (v1.0.5 / v1.18.0, Cocos2d-x + Box2D) and the hardware constraints of the PSP.

While recent reverse-engineering efforts successfully extracted ground-truth physics constants and exact mathematical stage curves from `libgame.so`, the current codebase is an early monolithic prototype tightly coupled to PSP hardware registers. It lacks the dual-build PC abstraction, relies on heavy runtime decoding (PNG and OGG via `stb`), hardcodes parameters in C++ headers, and does not employ static memory pooling.

---

## 2. Component-by-Component Analysis

### 2.1 Physics Engine (`src/physics/`)
- **Current State:**
  - `PhysicsWorld.cpp`: Wraps `b2World` with gravity set per stage. Implements basic `b2ContactListener` detecting driver head collision with terrain and item sensor triggers.
  - `PlayerCar.cpp`: Builds chassis as a 6-vertex polygon, 2 wheel circles connected via `b2WheelJoint`, and driver head via `b2RevoluteJoint`. Implements speed-dependent torque dampening and airborne rotational torque.
  - `Terrain.cpp`: Implements exact decompiled formulas for `StageCountrySide`, `StageDesert`, `StageArctic`, and `StageMoon`. Uses a sliding window array (`mPointsX`, `mPointsY`) and a single static `b2Body` with edge shapes.
- **What to Reuse:**
  - Exact chassis polygon vertices and wheel offsets extracted from `PlayerCar::create` (`0x000d2660`).
  - Exact stage height formulas for Countryside, Desert, Arctic, Moon from `libgame.so`.
  - Base suspension frequency (3.0 Hz) and damping ratio (0.20).
  - Floating-point control register setup in `fpu.h` (`fpu_no_exceptions()`), which prevents FPU crashes from Box2D subnormal divisions.
- **What Must Be Refactored / Replaced:**
  - **Memory Allocations:** Box2D currently uses standard `b2BlockAllocator` and heap `malloc`. A dedicated static pool (`b2Alloc`/`b2Free`) must be installed to guarantee zero allocations during races.
  - **Terrain Representation:** The single static body with rebuilt edge fixtures risks collision glitches and edge-catching. It must be converted into chunked `b2ChainShape` objects (30–40m chunks) pooled and recycled as the car advances.
  - **Item Management:** Collectibles are currently kept in a fixed linear array with repeated scans. Must be organized per terrain chunk.
  - **Driver Simulation:** Current neck joint is a simple revolute joint. Needs a soft spring angle limit and torso/body segment matching the original ragdoll.

### 2.2 Graphics & Rendering (`src/graphics/`)
- **Current State:**
  - `Renderer2D.cpp`: Directly emits PSP GU display list commands (`sceGuDrawArray`) using 2D and 3D vertex structs (`Vertex2D`, `ColorVertex2D`).
  - `Texture.cpp`: Uses `stb_image.h` (`stbi_load`) at runtime to decompress PNG files from disk into 32-bit linear RGBA buffers.
  - `Font.cpp`: Parses BMFont text format (`.fnt`) and renders glyph quads.
- **Deficiencies & Critical Bottlenecks:**
  - **Hardware Lock-in:** `Renderer2D` is 100% PSP-specific. There is zero PC rendering implementation, blocking immediate iteration, automated testing, and fidelity calibration.
  - **Runtime PNG Overhead:** Loading 15+ PNGs via `stbi_load` takes seconds on PSP's 333 MHz CPU and wastes MBs of heap memory. Textures are unswizzled (linear), crippling PSP GPU cache fill rate.
  - **No Texture Atlas / Batching:** Each sprite switches textures and submits tiny draw calls.
- **What Must Be Refactored / Replaced:**
  - Introduce an abstract graphics interface (`IRenderer2D`) or platform backend: `src/platform/psp/GuRenderer.cpp` and `src/platform/pc/GLRenderer.cpp`.
  - Replace runtime PNG loading with precompiled, swizzled PSP texture assets (`.tex` / `.bin`) and atlas sheets (`.atl`) generated offline via `tools/convert_assets.py`.
  - Implement quad batching to reduce draw calls per frame from >150 to <10.
  - Use low-res font (`gamefont_lowres`, 256x256) on PSP to preserve VRAM.

### 2.3 Audio System (`src/audio/`)
- **Current State:**
  - `AudioEngine.cpp`: Spawns a dedicated PSP kernel thread (`sceAudioChReserve`). Loads raw OGG audio files via `stb_vorbis_decode_filename` into memory buffers. Mixes samples in software with fixed-point pitch calculation for engine RPM sound.
- **Deficiencies & Bottlenecks:**
  - `stb_vorbis` decoding at startup consumes ~4 MB of RAM and several seconds of CPU time.
  - Software mixing wastes CPU cycles on the single Allegrex core.
  - Motor sound pitch calculation has audible stepping and lack of smooth filtering.
  - No PC audio implementation (SDL2 mixer).
- **What Must Be Refactored / Replaced:**
  - Platform abstraction: `src/platform/psp/PspAudio.cpp` (using `sceSas` hardware ADPCM voices for motor and SFX, streaming double-buffered audio for music) and `src/platform/pc/SdlAudio.cpp`.
  - Offline conversion of SFX to ADPCM/VAG and WAV, eliminating `stb_vorbis`.

### 2.4 Core, Configuration & Save (`src/core/`)
- **Current State:**
  - `System.cpp`: Manages PSP exit callbacks, display buffers in VRAM, and `sceCtrlReadBufferPositive`.
  - `GameConstants.h`: Contains all physical constants, stage coefficients, and vehicle geometries as hardcoded `#define`s.
  - `PlayerInfo.cpp`: Saves 4 upgrade levels, stage records, and coins into `save.dat` with simple `fwrite`/`fread`.
- **Deficiencies:**
  - Hardcoded parameters violate the data-driven architecture requirement. Iterative calibration requires modifying C++ code and recompiling every time.
  - Save file has no version header, no magic number, no CRC32 checksum, and no `.bak` backup file (vulnerable to corruption if powered off during write).
  - No PC event loop or input abstraction.
- **What Must Be Refactored / Replaced:**
  - Move all tunable parameters to external text configuration files: `data/vehicles.cfg`, `data/stages.cfg`, `data/physics.cfg`.
  - Implement a zero-allocation INI/CFG parser in `src/core/Config.cpp`.
  - Implement robust atomic save system matching Vita port spec (magic, version, CRC32, atomic rename, `.bak` fallback).
  - Abstract input: `IInput` implemented by `src/platform/psp/PspInput.cpp` and `src/platform/pc/SdlInput.cpp`.

---

## 3. Platform & Hardware Risks on PSP

| Risk Factor | PSP Constraint | Impact on HCR | Mitigation Strategy |
| :--- | :--- | :--- | :--- |
| **RAM Budget** | 32 MB user space (PSP-1000) | Heap exhaustion, crash on stage reload | Zero runtime `malloc`/`new`. Static memory pools for Box2D bodies, particles, terrain chunks. |
| **VRAM Budget** | 2 MB eDRAM (512 KB per 512x512x32b buffer) | Double-buffer + Z-buffer takes 1.5 MB; only 512 KB VRAM left for textures | Keep textures in main RAM with GE DMA transfer, or use 16-bit RGBA5650/4444 textures in VRAM. |
| **Texture Cache** | 8 KB GPU texture cache | Linear textures suffer severe cache misses | Swizzle all textures offline into PSP 16x8 pixel tile layout. |
| **CPU Performance** | 333 MHz Allegrex (single core) | Physics + particle simulation must finish within 5 ms | Avoid `double` in hot loop; compile with `-O2 -G0 -fno-exceptions -fno-rtti`; Box2D velocity iterations=8, position=3. |
| **FPU Quirks** | MIPS FCR31 raises SIGFPE on subnormal float / div-by-zero | Box2D joint solvers crash immediately on zero relative velocity | Keep `fpu_no_exceptions()` enabled at startup. |
| **Storage I/O** | Memory Stick read speed ~2-5 MB/s | Audio/texture stutters during gameplay | Preload all level assets during loading screen; stream music in 16 KB chunks; no blocking I/O during races. |

---

## 4. Planned Project Re-Architecture

```
hcr-psp/
├── data/                       <- Runtime data configs (zero-allocation parser)
│   ├── physics.cfg
│   ├── vehicles.cfg
│   └── stages.cfg
├── docs/                       <- Documentation, calibration protocols & audit
│   ├── audit.md
│   ├── assets_report.md
│   ├── params.csv
│   └── reference.md
├── tools/                      <- Offline asset & calibration toolchain
│   ├── convert_assets.py       <- Unpack, swizzle textures, convert audio
│   └── decompile_stages.py
├── src/
│   ├── core/                   <- Platform-agnostic game logic
│   │   ├── Config.h / .cpp     <- Flat zero-alloc CFG parser
│   │   ├── Game.h / .cpp       <- Main gameplay loop & state machine
│   │   ├── SaveSystem.h / .cpp <- Atomic CRC32 save storage
│   │   └── Types.h
│   ├── physics/                <- Platform-agnostic Box2D simulation
│   │   ├── PhysicsWorld.h / .cpp
│   │   ├── MemoryPool.h / .cpp <- Fixed b2Alloc pool
│   │   └── ContactListener.h / .cpp
│   ├── vehicle/                <- Vehicle definitions, suspension & control
│   │   ├── Vehicle.h / .cpp
│   │   ├── Driver.h / .cpp
│   │   └── VehicleDefs.h
│   ├── terrain/                <- Procedural terrain curves & chunk manager
│   │   ├── TerrainManager.h / .cpp
│   │   ├── TerrainCurves.h / .cpp
│   │   └── TerrainChunk.h / .cpp
│   ├── platform/               <- Hardware abstraction layer
│   │   ├── Platform.h          <- Interfaces: IRenderer, IAudio, IInput, IFileSystem
│   │   ├── psp/                <- PSP implementation (sceGu, sceAudio, sceSas, sceCtrl)
│   │   │   ├── PspRenderer.cpp
│   │   │   ├── PspAudio.cpp
│   │   │   ├── PspInput.cpp
│   │   │   └── PspSystem.cpp
│   │   └── pc/                 <- PC implementation (SDL2, OpenGL, ImGui telemetry)
│   │       ├── PcRenderer.cpp
│   │       ├── PcAudio.cpp
│   │       ├── PcInput.cpp
│   │       └── PcSystem.cpp
│   └── main.cpp                <- Universal entry point
├── Makefile                    <- PSP toolchain build (psp-gcc / psp-g++)
└── CMakeLists.txt              <- Dual-build (PC native build & PSP cross-compilation)
```

---

## 5. Milestone M0 Action Plan

1. **Asset Inventory (`docs/assets_report.md`):** Complete catalog of APK textures, audio, fonts, and particle sheets.
2. **Calibration Tracking (`docs/params.csv` & `docs/reference.md`):** Establish ground truth parameter origins.
3. **Data Configs (`data/*.cfg`):** Extract hardcoded values into `vehicles.cfg`, `stages.cfg`, `physics.cfg`.
4. **Asset Converter (`tools/convert_assets.py`):** Convert textures to swizzled format, generate font descriptors, prepare audio.
5. **Dual-Build Setup:** Create CMakeLists and modular build targets for PC (SDL2 + GL) and PSP (`EBOOT.PBP`).
6. **M0 Demo:** Verify Gray Jeep driving on procedural hills at locked 60 FPS in both PC and PSP builds without runtime allocations.

