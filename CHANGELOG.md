# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- **T-010**: Static zero-allocation Box2D memory pool.
- **T-011**: Calibration of chassis inertia and center-of-mass.
- **T-012**: Wheel joint spring frequency and suspension travel tuning.
- **T-020**: Recycled terrain chunks (`b2ChainShape`).

---

## [0.1.0-beta.3] - 2026-11-15

### Added
- **Full 1.30.0 Content Parity (28 Stages & 29 Vehicles):** Complete roster parity with official APK 1.30.0, including all 28 stages (`countryside` through `nuclear`) and 29 vehicles (`jeep` through `sleigh`).
- **Specialized Vehicle Physics Architectures (T-131b):**
  - **Snowmobile (Mode 1):** Front ski runner terrain sliding contact with directional friction and rear rubber caterpillar propulsion.
  - **Hovercraft (Mode 2):** Contact-free dynamic air cushion spring-damper repulsion, hidden support wheels (`hide_wheels = 1`), and lateral air rudder turning moments.
  - **Sleigh (Mode 1):** Dual ski runner low-friction snow sliding with hidden support wheels (`hide_wheels = 1`).
  - **Moonlander (Mode 3):** Active vertical rocket thrust propulsion controlled via `Triangle` / `D-pad Up` (`SetThruster(bool)`), dynamic fuel consumption, and attitude stabilization.
- **Streaming Music Engine & Audio Architecture (H16, T-132, H17b, H17c):**
  - Lock-free Single-Producer Single-Consumer (SPSC) streaming ring buffer with hardware memory barriers (`__sync_synchronize()`), zero underruns over 60s stress test.
  - Offline 16-bit mono 22050 Hz PCM streaming pipeline with fallback `.ogg` $\to$ `.wav` path normalization in `AudioEngine::PlayMusic`.
  - Independent channel routing and volume controls (`mMusicVolume` for music, `mSFXVolume` for engine/SFX).
- **Interactive Pause Modal & Distance Milestones (T-133):**
  - State machine pause mode (`STATE_PAUSE`) triggered via `Start` with Resume, Restart, and Exit options.
  - Periodic distance milestone rewards (+500 coins every 100 meters).
- **Stage Texture Path Resolution & Asset Audit (H17a, T-052b):**
  - Resolved texture paths in `config/stages.cfg` for 19 stages to match original APK naming conventions.
  - Comprehensive asset integrity audit in `tools/convert_assets.py` covering 56 stage textures, 87 vehicle textures, and 28 music tracks.
- **PPSSPP Hardware Re-Validation & Bit-Exact Hashes (T-052b):**
  - 100% bit-exact heightfield FNV-1a 64-bit determinism across all 28 stages (`match=YES`).
  - Authenticated performance telemetry (`docs/bench/perf_snapshot.txt`) and visual validation suite.
- **24 Automated CI Regression Test Suites:** Full green coverage across physics, content, audio streaming, and save systems.

### Fixed
- **PlayMusic Audio File Opening:** Resolved `.ogg` file presence blocking `.wav` fallback by verifying RIFF/WAVE container format inside `TryOpenWavFile`.
- **Audio Thread Race Condition:** Converted `sMusicRingBuffer` to true lock-free SPSC buffer, removing non-atomic counters and decoupling stream reading from callback thread.
- **Texture Load Errors:** Fixed broken ground/surface texture paths across 19 stages in `stages.cfg`.

---

## [0.1.0-beta.2] - 2026-11-14

### Added
- **Multi-Vehicle Support (T-032 / T-120):** Data-driven architecture in `PlayerCar` supporting 4 playable vehicles (`jeep`, `motocross`, `monster_truck`, `race_car`) with authentic physics parameters and APK 1.30.0 textures.
- **Drivetrain Types & 4th Upgrades (T-032):** Implemented RWD, FWD, and AWD drive types; 4th upgrade specializations strictly aligned to vehicle configs: 4WD torque distribution ratio (Hill Climber & Monster Truck), Mid-air pitch control authority (Motocross Bike), and Aerodynamic downforce wing (Race Car).
- **Physical Vehicle Calibration:** Accurate Box2D wheel radii ($R_{\text{jeep}}=0.40$ m, $R_{\text{moto}}=0.45$ m, $R_{\text{monster}}=0.65$ m, asymmetric $R_{\text{race\_front}}=0.36$ m / $R_{\text{race\_rear}}=0.42$ m) and calibrated chassis densities ($\rho \in [1.00, 2.00]$, Box2D chassis mass $\approx 1.03$–$4.86$ kg).
- **3-Tab Garage & Progression Economy (T-031):** Interactive Stages, Vehicles, and Upgrades carousel interface with quadratic cost curve $C(lvl) = 1000 \cdot (lvl+1)^2$, lazy-loading textures ($<0.2$ MB footprint), and race start authorization guarding locked content.
- **Binary Save System with CRC32 (T-030):** High-integrity save format (`HCRS`) with natural 4-byte MIPS alignment, IEEE 802.3 CRC32 checksum, atomic `.tmp` $\to$ `.bin` rotation, `.bak` corruption failover recovery, and legacy `'HCR1'` save migration.
- **Driver Head Ragdoll & Driver Down (T-015):** Dynamic head body with `b2RevoluteJoint` neck limits `[-0.60, +0.60]` rad, restoring PD spring motor, weakened motor torque on death for limp ragdoll, and solid terrain contact detection triggering instant `DRIVER DOWN!` while keeping collectible sensors immune.
- **Complete Trick Bonus Suite (T-113):** Air Time bonus ($\ge 1.0$s), Wheelie bonus ($\ge 1.5$s on rear wheel), Backflip/Frontflip detection, animated HUD stunt banner display with coin rewards, and automatic career coin persistence.
- **PSP-Native Swizzled Texture Pipeline (T-111):** Eliminated runtime PNG decoding via offline `.tex` binary format (`RGBA8888` for chassis/wheels, `RGBA5551` for UI/icons), saving 89% code footprint in `Texture.o` and $>2.0$ MB heap on PSP.
- **Cross-Platform Heightfield Determinism (T-112):** Bit-exact FNV-1a 64-bit stage height hashes verified between Linux x86_64 and PSP MIPS Allegrex over 2,000 meters.
- **Amortized Terrain Chunk Generation (T-119):** Reduced peak frame time from 10.9 ms to 1.9 ms via rolling 8-vertex incremental generation per physics step.
- **Fixed Physics Timestep Accumulator (T-108):** Fix-your-timestep accumulator with spiral-of-death protection and render transform interpolation.
- **14 Automated CI Regression Tests:** Expanded test suite with 100% green coverage across all physical, procedural, progression, and economic subsystems.

### Fixed
- **Clean Path Hygiene (H1 / T-102):** Removed all absolute host paths and personal usernames across tracked source code, documentation, and tools.
- **PSP Heap Configuration (H2 / T-116):** Replaced hardcoded static 24 MB allocation with `PSP_HEAP_SIZE_KB(-1024)` to safely support 32 MB PSP-1000 hardware.
- **Air Control Torque Direction (T-103):** Gas pedal correctly tilts nose up (CCW backflip) and brake tilts nose down (CW frontflip).
- **Out-of-Fuel Coasting (T-104):** Engine disconnects on empty fuel allowing natural inertia coasting; game over triggers only upon complete vehicle stop.
- **In-Air Sensor Collision Filtering (T-105):** Airborne state correctly ignores collectible item triggers.
- **Item Lifecycle & Memory Leak (T-106):** Ring buffer deferred destruction queue and distance-based despawn behind car ($carX - 40$m).
- **Seam Collision Immunity (T-107):** Smooth chunk transitions utilizing `b2ChainShape` ghost vertex connectivity (`SetPrevVertex`/`SetNextVertex`).
- **Suspension Travel Limits (T-109):** Backported Box2D `b2WheelJoint` translation limits preventing wheel penetration on hard impacts.
- **Save Alignment Invariants (H4):** Enforced `static_assert(offsetof(...) % 4 == 0)` on all 32-bit fields for MIPS Allegrex safety.
- **Hash Consistency (H5):** Unified `FNV1a_32` hashing across save and config systems strictly to canonical lowercase string IDs.

### Changed
- Re-scoped project description from "clean-room" to "fan-made RE-assisted native reimplementation" targeting official APK 1.30.0 parity.
- Pruned ~3 MB of unused vendor Box2D samples and documentation while preserving core solver and licensed patches (`docs/VENDOR_PATCHES.md`).

---

## [0.1.0-beta.1] - 2026-11-13

### Added
- **Clean-room reimplementation** of Hill Climb Racing for Sony PlayStation Portable (PSP) and PC.
- **Dual-Build Framework:** Native PC target (SDL2 + OpenGL) and PSP target (PSPSDK + `sceGu` + `sceAudio`).
- **Exact mathematical curves** decompiled from `libgame.so` for Countryside, Desert, Arctic, and Moon stages.
- **Kinematic Box2D vehicle model**: 6-vertex chassis polygon, front and rear wheel joints, suspension damping, and basic air pitch control.
- **Data-driven configuration engine**: Extracted all tunable constants into `config/physics.cfg`, `config/vehicles.cfg`, and `config/stages.cfg`.
- **Missing assets detection**: Friendly on-screen instruction screen when running without converted game assets.
- **Offline Asset Converter**: Idempotent pipeline (`tools/convert_assets.py`) for unpacking user APKs and swizzling textures into PSP hardware tile format.
- **Automated CI/CD**: GitHub Actions workflow for building both PSP `EBOOT.PBP` and PC Linux binaries.
