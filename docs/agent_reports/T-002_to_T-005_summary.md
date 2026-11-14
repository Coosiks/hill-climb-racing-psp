# Milestone M0 Historical Tasks Retrospective Summary (T-002 to T-005)

**Date:** 2026-11-14  
**Author:** Antigravity / Senior C++/PSP-Homebrew Engineer  
**Scope:** Milestone M0 Documentation Debt Closure  

---

## 1. Overview
During Milestone M0 (Architecture, Dual-Build & Baseline Audit), foundational architectural tasks were executed prior to the introduction of individual `docs/agent_reports/T-xxx.md` reporting standards (established starting with T-100). This retrospective summary closes the documentation debt for tasks T-002 through T-005, providing auditable traceability for these completed milestones.

---

## 2. Summary of Tasks

### T-002: Asset Inventory & Memory Budgeting (`docs/assets_report.md`)
- **Objective:** Establish the VRAM and system RAM budget for the PSP target (32 MB system RAM on PSP-1000, 2 MB EDRAM/VRAM) by analyzing original game assets.
- **Deliverables:**
  - `docs/assets_report.md`: Complete audit of original APK texture dimensions, formats, and memory footprint.
  - VRAM layout design allocating space for front/back framebuffers (512x272x16bpp = 272 KB each), depth buffer (272 KB), and texture cache slot (up to 1 MB).
  - Identification of critical sprites (vehicle chassis, wheels, terrain surface, ground fill, driver head/body).

### T-003: External Data-Driven Configuration System (`config/*.cfg`)
- **Objective:** Decouple vehicle, stage, and physics parameters from compiled code to enable rapid balancing and multi-vehicle support.
- **Deliverables:**
  - `config/physics.cfg`: Fixed timestep, PTM ratio, iteration counts, fuel drain rates, camera smoothing parameters.
  - `config/vehicles.cfg`: Polygon vertices, wheel offsets/radii, spring frequency/damping, motor speed/torque, and upgrade cost multipliers.
  - `config/stages.cfg`: Stage gravitational acceleration, surface friction coefficients, coin reward multipliers, and asset bindings.
  - Runtime parser implementation in `src/core/Config.cpp` with zero dynamic allocation.

### T-004: Offline Asset Processing & Swizzling Pipeline (`tools/convert_assets.py`)
- **Objective:** Eliminate runtime image decoding on PSP by preprocessing textures offline into PSP native formats.
- **Deliverables:**
  - `tools/convert_assets.py`: Python CLI utility using Pillow.
  - Texture dimensions padded to next Power-of-Two (POT) width and height as required by PSP GE (Gu).
  - Downsampling of oversized images (>512 px) using Lanczos filtering.
  - Conversion to uncompressed 16-bit (`RGBA5551`) and 32-bit (`RGBA8888`) binary formats with PSP hardware block swizzling (16-byte aligned tile reordering) to optimize GPU texture cache locality.

### T-005: "Bring Your Own APK" Missing Asset Screen & Fault Tolerance
- **Objective:** Prevent silent crashes, assertion failures, or black-screen hangs when running on clean checkouts without commercial assets.
- **Deliverables:**
  - Fallback error rendering in `src/core/Game.cpp` and `src/graphics/Texture.cpp`.
  - Missing asset overlay informing the player to extract assets from a legally obtained APK into the `assets/` directory.
  - Safe null-handling in physics world creation and renderer when textures fail to load (ensuring headless tests and CI verification run without requiring proprietary assets).

---

## 3. Verification & Current Status
All four components (asset converter, configuration parsers, asset report, missing asset fault tolerance) are fully operational and continuously validated by the 13 automated PC regression tests and PSP build CI.
