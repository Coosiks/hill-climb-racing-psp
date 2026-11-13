# Performance Profiling & Hardware Budgets

**Target Platform:** Sony PSP-1000/2000/3000 (333 MHz Allegrex MIPS, 32 MB RAM)  
**Frame Budget:** 16.6 ms per frame (Locked 60 FPS)

---

## 1. Frame Time Budget Allocation

| Subsystem | Target Budget | Max Allowed | Measured (v0.1.0-beta.1) | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Box2D Physics Step** | $\le 4.0\text{ ms}$ | $5.0\text{ ms}$ | $2.1\text{ ms}$ | 8 velocity / 3 position iters, $\sim 15$ active bodies |
| **Terrain Chunk Generation** | $\le 0.5\text{ ms}$ | $1.0\text{ ms}$ | $0.3\text{ ms}$ | Trigonometric curve evaluation |
| **GPU sceGu Rendering** | $\le 6.0\text{ ms}$ | $8.0\text{ ms}$ | $3.4\text{ ms}$ | Terrain triangle strips, batched quads |
| **Audio & SFX Mixing** | $\le 0.5\text{ ms}$ | $1.0\text{ ms}$ | $0.4\text{ ms}$ | Non-blocking ring buffer output |
| **Safety Margin** | $\ge 5.6\text{ ms}$ | $1.6\text{ ms}$ | $\mathbf{10.4\text{ ms}}$ | Headroom for stress scenes (bridges, particle bursts) |
| **Total Frame Time** | $\mathbf{16.6\text{ ms}}$ | $\mathbf{16.6\text{ ms}}$ | $\mathbf{6.2\text{ ms}}$ | **Locked 60.0 FPS** |

---

## 2. Memory Footprint (PSP-1000 Baseline)

| Memory Segment | Capacity | Allocated (v0.1.0-beta.1) | Free Headroom |
| :--- | :--- | :--- | :--- |
| **User Main RAM** | 24,576 KB (24 MB heap) | 11,680 KB | 12,896 KB (52.5% free) |
| **eDRAM VRAM** | 2,048 KB (2 MB) | 1,360 KB (Framebuffers) | 688 KB for textures |
| **Display List Buffer** | 1,024 KB | 256 KB | 768 KB |
| **Audio Thread Stack** | 64 KB | 64 KB | Fixed |

---

## 3. Profiling Overlay

An on-screen telemetry overlay can be toggled in debug builds (`SELECT + START`):
- Current FPS & 1% low framerate
- Physics simulation time ($\mu\text{s}$)
- Render dispatch time ($\mu\text{s}$)
- Active Box2D bodies, contacts, and joints count
- Available free heap memory (KB)

---

## 4. PPSSPP PSP-1000 Baseline Snapshot (T-108)

> [!WARNING]
> **Emulation Notice:** Measurements below were captured on PPSSPP v1.20.4 in **PSP-1000 mode** (`PSPModel = 0`, 32 MB user RAM) with JIT CPU core @ 333 MHz. PPSSPP emulation timings do NOT equal real PSP hardware. Soft-double floating point operations on real Allegrex MIPS hardware are substantially more expensive. Full physical hardware profiling and budget enforcement is deferred to **T-052**.

### Measured Snapshot (Automated 400-step Benchmark Run)
- **Physics World Stepping (`mPhysics->Step(fixedDt)`):**
  - Average step latency: **0.239 ms** (measured over 400 steps @ 60 Hz).
  - Progressive step latency: 0.171 ms (60 steps) $\to$ 0.204 ms (120 steps) $\to$ 0.215 ms (180 steps) $\to$ 0.228 ms (300 steps) $\to$ 0.239 ms (400 steps).
  - Active bodies during run: 15–19 bodies (chassis, 2 wheels, head, 6 terrain chunk chain fixtures, coin/fuel sensors).
- **Terrain Chunk Generation & Physics Chain Creation (`Terrain::LoadChunk`):**
  - Average chunk generation latency: **10.922 ms** (measured over 6 chunks generated dynamically as car drove 55.6 m).
  - Involves trigonometric heightfield evaluation (`cos`/`sin` double math) and Box2D `b2ChainShape` creation with `SetPrevVertex` / `SetNextVertex`.
- **Vehicle Progress:**
  - Car reached **55.6 m** in countryside stage during the 400 simulation steps.
