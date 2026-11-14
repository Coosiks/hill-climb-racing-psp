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
- [x] **T-113 (game, economy):** Implement complete trick bonus suite (Air Time, Wheelie, Flips) and 14th automated CI regression test (`test_tricks_and_driver`).
- [x] **T-114 (docs, vendor):** Prune unused Box2D samples/docs, audit params.csv classification, close M0 documentation debt (`T-002_to_T-005_summary.md`).
- [x] **T-115 (release):** Release v0.1.0-beta.2 with updated notes, changelog, and clean artifacts (next release milestone).
- [x] **T-116 (core):** Fix PSP-1000 heap allocation budget: replace fixed 24 MB request with dynamic allocation (`PSP_HEAP_SIZE_KB(-1024)`).
- [x] **T-117 (physics):** Quadratic aerodynamic air drag force $\vec{F}_{\text{drag}} = -c_{\text{drag}} \|\vec{v}\| \vec{v}$ on vehicle chassis with configurable coefficient in `physics.cfg`.
- [ ] **T-118 (test):** Tighten seam crossing deltaVx metric in `test_terrain.cpp` (isolate natural curvature gradient from ghost-jolt collisions).
- [x] **T-119 (perf):** Amortize terrain chunk generation into 8-vertex slices per step, eliminating the 10.9 ms frame spike on PSP-1000.
- [x] **H3 (evidence):** Commit authentic PSP benchmark telemetry outputs (`heightfield_hashes_psp.txt` & `perf_snapshot.txt`) to `docs/bench/`.
- [x] **H4 (save):** Enforce MIPS Allegrex 4-byte natural alignment static assertions in `PlayerInfo.h`, sync 64-capacity limits, and fix T-030 report errata.
- [x] **H5 (audit):** Audit FNV-1a 32-bit ID hash consistency across save and config systems, canonicalizing to lowercase string IDs.

---

## [ ] Milestone M1: Physics Engine Calibration & Vehicle Dynamics
- [ ] **T-010 (physics):** Implement zero-allocation static memory pool for Box2D allocations (`b2Alloc`/`b2Free`).
- [ ] **T-011 (vehicle):** Refine Jeep chassis mass, rotational inertia, and center-of-mass placement according to decompiled parameters.
- [ ] **T-012 (vehicle):** Implement exact wheel joint suspension damping, spring frequency scaling, and travel limits.
- [ ] **T-013 (vehicle):** Implement speed-dependent motor torque curve and free-wheeling idle behavior.
- [ ] **T-014 (vehicle):** Implement in-air angular pitch torque (wheelie & backflip air control) calibrated against 60 FPS reference video.
- [x] **T-015 (vehicle, physics):** Replicate driver ragdoll physics (head revolute joint, neck spring constraint, terrain collision instant-loss condition).
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
- [x] **T-040 (render):** Hardware-accelerated static sprite quad batcher in `Renderer2D` (up to 256 quads / 1536 verts) reducing text/sprite draw calls to 1 per texture.
- [x] **T-041 (render):** Multi-layer parallax background rendering with stage-specific skies and horizon hills.
- [x] **T-042 (render):** Static ring-buffer particle system for tire dust (`dirt.plist`) and snow particles (`snow.plist`).
- [x] **T-043 (audio, tools):** Offline audio preprocessing pipeline in `tools/convert_assets.py` converting APK audio to canonical 16-bit PCM mono 22050Hz WAV with `--no-audio` flag.
- [x] **T-044 (audio):** Fast zero-runtime-decode WAV loader, 16.16 fixed-point phase resampler engine loop with pitch modulation [0.50, 2.00], pause/resume semantics, and SFX mixing.

---

## [ ] Milestone M5: Game Flow, Polish & Gold Master
- [ ] **T-050 (ui):** In-game HUD: fuel gauge with low-fuel flashing warning, distance meter, coin counter, and stunt alerts.
- [ ] **T-051 (game):** Complete game-over flow (distance score, coins gathered, best stage record, retry/garage navigation).
- [ ] **T-052 (perf):** Final performance profiling on real PSP-1000 hardware at 333 MHz (verification of locked 60 FPS in stress scenes).
- [ ] **T-053 (release):** Production release v1.0.0 (signed EBOOT.PBP, release notes, packaging guide).

---

## [ ] Milestone M6: Content Parity 1.30.0
- [x] **T-120 (content, re):** Vehicles & Stages 1.30.0 reverse engineering: automated extraction script `extract_1300_info.py`, disassembly dump `re_1300_dump.txt`, content registry `hcr_1300_inventory.md` for 29 vehicles and 28 stages, and RGBA5551/RGBA8888 classification in `convert_assets.py`.
- [x] **T-121 (terrain, re):** Expand stages to 8 (`countryside`, `desert`, `arctic`, `moon`, `highway`, `cave`, `mars`, `forest`), extract height formulas from `libgame.so` 1.30.0, implement Cave ceiling with $\ge 4.5$m clearance and Driver Down neck break, add `has_ceiling`, `ceiling_offset`, `fuel_interval`, `has_bridges` to `stages.cfg` (100% config key coverage in `test_config`).
- [x] **T-021 (physics, terrain):** Implement zero-alloc suspension bridge pool (`MAX_ACTIVE_BRIDGES = 4`, `MAX_BRIDGE_PLANKS = 12`) on Forest stage, 10 dynamic planks connected by 11 `b2RevoluteJoint` with $[-0.25, +0.25]$ rad limits, verified zero body leaks on chunk unload, batched sprite rendering via `Renderer2D::DrawSprite`.
- [x] **T-122 (content, vehicle):** Expand vehicle roster in `config/vehicles.cfg` to 8 vehicles (`tractor`, `hippievan`, `quadbike`, `touristbus`), verified Box2D simulation masses and wheel radii, 4th upgrade slot types, and 1.30.0 unlock costs (`test_vehicles`).
- [x] **T-123 (content, terrain):** Expand stages to 12 (`mountain`, `mudpool`, `volcano`, `beach`), implement viscous mud damping (`has_mud_pools = 1`, `mud_damping`), verify 100% bit-exact frozen heightfield hashes for stages 0..7 over 4001 points (0..2000m).
- [x] **T-124 (content, vehicle):** Expand vehicle roster to 12 (`onewheeler`, `fingerscrew`, `policecar`, `ambulance`), implement 4th upgrade slot `4 = STABILITY` and `5 = BOOST`, reassign Tractor and Hippie Van, verified authoritative Box2D simulation mass table.
- [x] **T-033 (ui, hud):** Authentic Game Over race summary breakdown (collected coins, stunt coins, total balance), `NEW RECORD!` badge detection, garage upgrade card labels for STABILITY and BOOST, 18th automated regression test (`test_mud_and_roster12`).
- [x] **H9 (render):** Audit 32-bit ABGR color packing across PSP (`GU_COLOR_8888`) and PC OpenGL (`glColor4ub`), document canonical format invariant in `Renderer2D.h`, verify individual wheel diameters and textures in `PlayerCar::Render()`.
- [x] **T-125 (content, terrain):** Expand stages to 18 (`rollercoaster`, `night`, `rooftops`, `junkyard`, `construction`, `rainbow`), implement Night Mode (`Renderer2D::DrawLightCone`, ambient dark quad, chassis-angled headlight beam), particle dirt colors for 18 stages, 100% bit-exact baseline hashes.
- [x] **T-126 (content, vehicle):** Expand vehicle roster to 18 (`electric`, `dieseltruck`, `rallycar`, `dragster`, `firetruck`, `tank`), implement Dragster wheel asymmetry ($R_{rear}/R_{front} = 2.07\times > 1.40\times$), Tank chassis mass ($7.29$ kg), 4th slot mapping, authoritative 18-vehicle Box2D mass table.
- [x] **H10 (core, hygiene):** Pure `double` precision heightfield determinism in `highway`, `forest`, `mountain`, `mudpool`, `volcano` (eliminating `EXPECTED_BASELINE_HASHES_ALT`), canonical `[screwfinger]` vehicle ID migration (`0xDC088A10` $\to$ `0xB61B2D9E`), `.gitignore` review bundle hygiene.
- [x] **T-127 (test):** 19th automated regression test (`test_roster18_and_night`) verifying 18 stages, 18 vehicles, Dragster asymmetry, Night Mode batch limits, and save migration.
- [x] **H11 (core, render, save):** Save migration deduplication merging old `0xDC088A10` into canonical `0xB61B2D9E` (`vehicleCount--`, `fmaxf` levels), Night Mode render layer ordering (ambient darkening FIRST, additive headlight cone SECOND), world-to-screen camera projection in `Renderer2D::DrawLightCone`, honest batch/draw call metrics (`sBatchedQuadCount = 0`, `sDrawCallCount = 2`).
- [x] **T-128 (content, terrain):** Expand stages to 24 (`seasons`, `bootcamp`, `xmas`, `alienplanet`, `arcticcave`, `ragnarok`), pure `double` height generators, Arctic Cave ceiling collision, Ragnarok mud pools, dirt particle colors, 100% bit-exact frozen heightfield determinism for stages 0..17.
- [x] **T-129 (content, vehicle):** Expand vehicle roster to 24 (`dunebuggy`, `superoffroad`, `truck`, `minibike`, `bigfinger`, `hotrod`), 4th upgrade slot mapping (AWD, DOWNFORCE, AIR_CONTROL, BOOST), authoritative 24-vehicle Box2D physical mass table, APK 1.30.0 unlock costs.
- [x] **Test 20:** 20th automated regression test (`test_roster24`) verifying 24 stages, 24 vehicles, save migration deduplication, Night Mode screen coordinates, and layer ordering.
- [x] **H13 (core):** Unify stage string identifiers into file-scope `kStageTags[]` in `PlayerInfo.cpp`, compile-time `static_assert(sizeof(kStageTags)/sizeof(kStageTags[0]) == STAGE_COUNT)`, bounds-checked `PlayerInfo::StageTagAt(i)` accessor, null guards, and Sub-test 9 in `test_save.cpp`.
- [x] **H12 (render):** Audit blending and texture state restoration in `Renderer2D::DrawLightCone` across PSP (`GU_ADD, GU_SRC_ALPHA, GU_FIX`) and PC (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`), confirming persistent `GU_BLEND`/`GL_BLEND` activation.
- [x] **T-130 (content, terrain):** Expand stages to 28 (`factory`, `haunted`, `northpole`, `nuclear`), achieving 100% stage content parity with 1.30.0; pure `double` height generators, bridge gating on factory, dual night+mud mechanics on haunted.
- [x] **T-131a (content, vehicle):** Add `train` (Kiddie Express) as 25th vehicle; elongated wheelbase ($3.80$ m $> 3.40$ m), heavy chassis mass ($9.7367$ kg $> 3.4206$ kg median), BOOST 4th upgrade slot, elevated locomotive angular damping ($0.60$).
- [ ] **Test 21:** 21st automated regression test (`test_roster28`) verifying 28 stages, 25 vehicles, FNV-1a hashes, train mass & wheelbase, and H13 regressions.

---

## [ ] Milestone M7: Special Vehicle Physics & Content Parity Completion
### Batch 16 Step 0: Special Vehicle Physics Architectural Grounding
Specialized physics architectures required for 4 non-standard vehicles that do not fit the standard two-wheel `PlayerCar` chassis-and-wheels model:
- **Snowmobile:** Front ski steering runner with terrain edge contact vs rear continuous rubber caterpillar tread.
  - *Unresolved Design Question 1:* Model front ski as rigid slider polygon fixture with directional friction or revolute suspension joint; model rear tread as multi-wheel chain or single long friction strip.
- **Sleigh:** Dual sliding runners (skis) with zero rotating wheel bodies.
  - *Unresolved Design Question 2:* Contact modeling with two low-friction edge contact pads; snow/ice friction ($\mu = 0.05$) vs terrain drag on asphalt/dirt ($\mu = 0.80$).
- **Hovercraft:** Dynamic air cushion repulsion aerodynamics.
  - *Unresolved Design Question 3:* Air cushion lift force implementation (raycast-based spring repulsion vs Box2D soft contact sensor); zero-friction lateral drift and air rudder turning moments.
- **Moonlander:** Rocket thrust propulsion & reaction control system (RCS).
  - *Unresolved Design Question 4:* Main vertical rocket engine variable impulse vector; attitude control torque jets; multi-strut landing gear shock damping on harsh terrain.

*All future parameters for Batch 16 vehicles are classified as `design/TODO-TUNE` until calibrated against reference video.*



