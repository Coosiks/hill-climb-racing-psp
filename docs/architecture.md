# Architecture Specification: Hill Climb Racing Native PSP Port

## 1. High-Level Architectural Overview

The architecture is built on a clean-room, platform-agnostic design separating gameplay simulation from hardware-specific drivers.

```
                      +-----------------------------+
                      |         Game Loop           |
                      |   (Fixed 60 Hz Timestep)    |
                      +--------------+--------------+
                                     |
             +-----------------------+-----------------------+
             |                                               |
+------------v------------+                     +------------v------------+
|     Physics & World     |                     |    Rendering & Audio    |
| - Box2D Simulation      |                     | - Camera & Viewport     |
| - Procedural Terrain    |                     | - Terrain Fill & Ribbon |
| - Vehicle Kinematics    |                     | - Sprite Batching       |
| - Dynamic Suspension    |                     | - Motor Synthesizer     |
+------------+------------+                     +------------+------------+
             |                                               |
+------------v-----------------------------------------------v------------+
|                      Platform Abstraction Layer                         |
|   (System::BeginFrame, Renderer2D, Texture, AudioEngine, Config)        |
+----------------------------+--------------------------------------------+
                             |
             +---------------+---------------+
             |                               |
+------------v------------+     +------------v------------+
|      PSP Platform       |     |       PC Platform       |
| - sceGu / sceGum       |     | - SDL2 Window & Events  |
| - sceAudio / sceSas     |     | - OpenGL 2D Pipelines   |
| - sceCtrl Input         |     | - SDL Audio Device      |
| - Memory Stick I/O      |     | - Telemetry & Debug     |
+-------------------------+     +-------------------------+
```

---

## 2. Core Modules

### 2.1 Core Subsystem (`src/core/`)
- `System`: Universal application harness managing windowing, double-buffering, and controller polling. On PSP, configures the Allegrex CPU to 333 MHz and initializes hardware GE buffers in eDRAM.
- `Config`: Zero-allocation configuration parser loading `config/physics.cfg`, `config/vehicles.cfg`, and `config/stages.cfg`.
- `Game`: Central game coordinator managing finite state machines (`STATE_GARAGE`, `STATE_PLAYING`, `STATE_GAMEOVER`), stunt scoring, and camera tracking.
- `PlayerInfo`: Manages persistent user stats, high scores, coins, and upgrade tiers.

### 2.2 Physics Engine (`src/physics/`)
- `PhysicsWorld`: Encapsulates `b2World` with stage-dependent gravity vectors. Implements `b2ContactListener` detecting driver head trauma and sensor pickups.
- `PlayerCar`: Implements chassis polygon, front/rear `b2WheelJoint` suspension, speed-modulated motor acceleration, free-coasting idle, and in-air angular pitch impulses.
- `Terrain`: Procedural heightfield generation based on 1:1 mathematical trigonometric functions from `libgame.so`.

### 2.3 Graphics & Rendering (`src/graphics/`)
- `Renderer2D`: Immediate and batched 2D rendering subsystem supporting orthographic world camera and screen-space UI coordinates.
- `Texture`: Hardware-accelerated texture management. On PSP, textures are aligned and swizzled into $16\times 8$ byte tiles. On PC, textures are uploaded to OpenGL 2D targets.
- `Font`: Bitmap font rasterizer (`gamefont_lowres`) for zero-allocation HUD and menu text drawing.

### 2.4 Audio Subsystem (`src/audio/`)
- `AudioEngine`: Multi-channel sound effects and engine synthesizer. On PSP, audio mixing runs in a dedicated thread with VFPU access. Pitch modulation $[0.5, 2.0]$ reflects engine revolutions.

---

## 3. Data Flow & Configuration Formats

All simulation constants are stored in standard INI/CFG format:
```ini
[world]
ptm_ratio = 32.0
fixed_timestep = 0.0166667
velocity_iterations = 8
position_iterations = 3

[jeep]
chassis_density = 1.50
base_motor_speed = 28.0
base_motor_torque = 10.0
base_spring_freq = 3.0
base_spring_damp = 0.20
```
This guarantees that physics parameters can be calibrated and balanced without recompiling the executable.

---

## 4. Memory Management & Zero-Allocation Policy

On the PSP, dynamic heap allocations (`malloc`/`new`) during active gameplay are strictly prohibited to prevent memory fragmentation and GC pauses:
1. All Box2D joint and fixture instances are allocated from dedicated static pools.
2. Particle systems use pre-allocated static ring buffers.
3. Terrain point buffers are maintained in contiguous fixed arrays.
4. Total user memory usage is constrained to $< 16$ MB (well within the 24 MB user heap budget of PSP-1000).
