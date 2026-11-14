# Project Roadmap: Hill Climb Racing Native PSP Port

This roadmap outlines all milestones from M0 through M5. Each task has a unique identifier (`T-xxx`), descriptive tags, acceptance criteria, and completion status.

---

## [x] Milestone M0: Architecture, Dual-Build & Baseline Audit
- [x] **T-001 (legal, build):** Complete pre-publish legal audit, sanitize repository, establish dual-build architecture (PC SDL2/GL + PSP sceGu), and create release v0.1.0-beta.1.
- [x] **T-002 (docs):** Inventory original APK assets and establish VRAM/RAM budgets (`docs/assets_report.md`).
- [x] **T-003 (config):** Move hardcoded vehicle, stage, and physics parameters to data-driven external files (`config/*.cfg`).
- [x] **T-004 (tools):** Build idempotent offline asset conversion pipeline (`tools/convert_assets.py`) with power-of-two padding and PSP hardware swizzling.
- [x] **T-005 (core):** Implement robust "Bring Your Own APK" asset missing screen to prevent startup crashes.

---

## [ ] Milestone M0.2: Reviewer Fixes & Hardening (v0.1.0-beta.2)
- [x] **T-100 (ci):** Enforce CI integrity: `set -euo pipefail` on all bash steps, eliminate tee return-code masking, verify non-empty EBOOT/ELF, fail artifact upload on missing files.
- [x] **T-101 (build):** Fix PSP compilation in `src/main.cpp`, verify clean build on PSPSDK.
- [x] **T-102 (build):** Rebuild release binary from clean tree, verify on PPSSPP with screenshot.
- [x] **T-103 (physics):** Fix inverted in-air pitch control (gas = backflip/CCW, brake = frontflip/CW) and flip detectors.
- [x] **T-104 (game):** Implement vehicle coasting on empty fuel tank until $|v| < 0.3$ m/s.
- [x] **T-105 (physics):** Filter sensor contacts in `IsInAir()` to prevent coin arcs from disabling air control.
- [x] **T-106 (physics):** Deferred item destruction queue post-step, fix duplicate collection and body memory leak.
- [x] **T-107 (physics):** Replace `b2EdgeShape` with sliding window `b2ChainShape` chunks (30 m window).
- [x] **T-108 (core):** Implement fixed timestep accumulator with max substeps and render transform interpolation.
- [x] **T-109 (physics):** Enable suspension travel limits (`enableLimit`, translation bounds).
- [x] **T-110 (core):** Unify constants into `Config::Physics`, parse all vehicle vertices, head radii, and upgrade multipliers.
- [x] **T-111 (tools, render):** Rewrite `convert_assets.py` with CLI argparse, ZIP extraction, POT padding, Lanczos downscale >512, swizzled .tex pipeline and loader, eliminating runtime PNG decompression on PSP.
- [x] **T-112 (core):** Cross-platform heightfield determinism: verified bit-exact match between glibc and newlib soft-float (4 stages, 0-2000m, 4001 pts).
- [ ] **T-113 (tune):** Tune stunt bonuses (TODO-TUNE), add air-time bonus and fuel capacity upgrade.
- [x] **T-114 (docs, vendor):** Prune unused Box2D samples/docs, audit params.csv classification, close M0 documentation debt (`T-002_to_T-005_summary.md`).
- [ ] **T-115 (release):** Release v0.1.0-beta.2 with updated notes, changelog, and clean artifacts (next release milestone).
- [x] **T-116 (core):** Fix PSP-1000 heap allocation budget: replace fixed 24 MB request with dynamic allocation (`PSP_HEAP_SIZE_KB(-1024)`).
- [ ] **T-118 (test):** Tighten seam crossing deltaVx metric in `test_terrain.cpp` (isolate natural curvature gradient from ghost-jolt collisions).
- [x] **T-119 (perf):** Amortize terrain chunk generation into 8-vertex slices per step, eliminating the 10.9 ms frame spike on PSP-1000.
- [x] **H3 (evidence):** Commit authentic PSP benchmark telemetry outputs (`heightfield_hashes_psp.txt` & `perf_snapshot.txt`) to `docs/bench/`.
- [x] **H4 (save):** Enforce MIPS Allegrex 4-byte natural alignment static assertions in `PlayerInfo.h`, sync 64-capacity limits, and fix T-030 report errata.

---

## [ ] Milestone M1: Physics Engine Calibration & Vehicle Dynamics
- [ ] **T-010 (physics):** Implement zero-allocation static memory pool for Box2D allocations (`b2Alloc`/`b2Free`).
- [ ] **T-011 (vehicle):** Refine Jeep chassis mass, rotational inertia, and center-of-mass placement according to decompiled parameters.
- [ ] **T-012 (vehicle):** Implement exact wheel joint suspension damping, spring frequency scaling, and travel limits.
- [ ] **T-013 (vehicle):** Implement speed-dependent motor torque curve and free-wheeling idle behavior.
- [ ] **T-014 (vehicle):** Implement in-air angular pitch torque (wheelie & backflip air control) calibrated against 60 FPS reference video.
- [ ] **T-015 (vehicle):** Replicate driver ragdoll physics (head revolute joint, neck spring constraint, terrain collision instant-loss condition).
- [ ] **T-016 (perf, tune):** PC in-game telemetry overlay with live parameter sliders and metrics export (`docs/perf.md`).
- [ ] **T-017 (tune):** Execute formal 0–100 m, max slope, and flip duration calibration protocol (`docs/params.csv`).

---

## [ ] Milestone M2: Terrain Generation, Chunks & Stage Set-Pieces
- [ ] **T-020 (terrain):** Transition terrain physics from monolithic array to recycled `b2ChainShape` chunks (30–40m chunks).
- [ ] **T-021 (terrain):** Validate cross-platform PRNG determinism between PC and PSP (identical seed produces identical heightfield hash).
- [ ] **T-022 (terrain):** Implement dynamic suspension bridge set-piece with joined wooden planks.
- [ ] **T-023 (terrain):** Implement chunk-based collectible pooling (coins 5/25/100/500 and fuel canisters).
- [ ] **T-024 (terrain):** Integrate stage-specific physical properties (Earth $g=-10$, Moon $g=-3.5$, Arctic $\mu=0.15$).

---

## [ ] Milestone M3: Vehicles, Garage, Upgrades & Persistent Save System
- [x] **T-030 (save):** Implement atomic key-value save storage with magic header, versioning, CRC32 checksum, and `.bak` fallback.
- [x] **T-031 (garage):** Garage menu system with upgrade purchasing (Engine, Suspension, Tires, 4WD) and stage selector.
- [x] **T-032 (vehicle):** Multi-vehicle architecture in `PlayerCar` with drivetrain types, specialized 4th-slot mechanics, and initial 4 vehicles (Jeep, Motocross, Monster Truck, Race Car) via `vehicles.cfg`.

---

## [ ] Milestone M4: Rendering Polishing, Particles & Audio Synthesizer
- [ ] **T-040 (render):** Hardware-accelerated sprite quad batcher to reduce draw calls to $<10$ per frame on PSP.
- [ ] **T-041 (render):** Multi-layer parallax background rendering with stage-specific skies and horizon hills.
- [ ] **T-042 (render):** Static ring-buffer particle system for tire dust (`dirt.plist`) and snow particles (`snow.plist`).
- [ ] **T-043 (audio):** Hardware `sceSas` voice synthesizer for engine audio with continuous pitch modulation [0.5, 2.0].
- [ ] **T-044 (audio):** Streaming double-buffered background music playback and sound effect prioritization.

---

## [ ] Milestone M5: Game Flow, Polish & Gold Master
- [ ] **T-050 (ui):** In-game HUD: fuel gauge with low-fuel flashing warning, distance meter, coin counter, and stunt alerts.
- [ ] **T-051 (game):** Complete game-over flow (distance score, coins gathered, best stage record, retry/garage navigation).
- [ ] **T-052 (perf):** Final performance profiling on real PSP-1000 hardware at 333 MHz (verification of locked 60 FPS in stress scenes).
- [ ] **T-053 (release):** Production release v1.0.0 (signed EBOOT.PBP, release notes, packaging guide).

---

## [ ] Milestone M6: Content Parity 1.30.0
- [x] **T-120 (content, re):** Vehicles & Stages 1.30.0 reverse engineering: automated extraction script `extract_1300_info.py`, disassembly dump `re_1300_dump.txt`, content registry `hcr_1300_inventory.md` for 29 vehicles and 28 stages, and RGBA5551/RGBA8888 classification in `convert_assets.py`.

