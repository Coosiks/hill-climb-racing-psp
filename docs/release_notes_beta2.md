# Hill Climb Racing PSP — Release v0.1.0-beta.2 (Beta 2)

Fan-made native C++ reimplementation of *Hill Climb Racing* (Fingersoft) for Sony PlayStation Portable (PSP) and PC (Linux / SDL2).

> [!IMPORTANT]
> **Legal Disclaimer:** This is an independent, non-commercial fan-made native reimplementation. Hill Climb Racing is a registered trademark of Fingersoft Ltd. This release distribution contains **ZERO copyrighted or proprietary assets** (no textures, audio, fonts, or original game data). Players must extract assets from their legally owned copy of Hill Climb Racing Android APK (v1.30.0 or v1.0.5) using the included extractor script (`tools/convert_assets.py`).

---

## What's New in v0.1.0-beta.2

### 1. Authentic Multi-Vehicle Roster (APK 1.30.0 Content Parity)
4 Playable Vehicles strictly matched to `config/vehicles.cfg` and verified by Box2D physical simulation:

| Vehicle ID | Display Name | Wheel Radii (m) | Chassis Mass (kg) | Wheel Masses (kg) | Total Mass (kg) | Drive Type | 4th Upgrade Type |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `jeep` | Hill Climber | Front: `0.40`<br>Rear: `0.40` | `3.4088` | Front: `0.4021`<br>Rear: `0.4021` | `4.3035` | 4WD (`2`) | `4WD` (`0`) |
| `motocross` | Motocross Bike | Front: `0.45`<br>Rear: `0.45` | `1.0300` | Front: `0.3817`<br>Rear: `0.3817` | `1.8839` | RWD (`0`) | `AIR_CONTROL` (`2`) |
| `monster_truck` | Monster Truck | Front: `0.65`<br>Rear: `0.65` | `4.8600` | Front: `1.3273`<br>Rear: `1.3273` | `7.6051` | 4WD (`2`) | `4WD` (`0`) |
| `race_car` | Race Car | Front: `0.36`<br>Rear: `0.42` | `2.8420` | Front: `0.3054`<br>Rear: `0.4156` | `3.6535` | RWD (`0`) | `DOWNFORCE` (`3`) |

- **Special 4th Upgrades:**
  - `jeep`: 4WD torque distribution ratio (`fourth_upgrade_type = 0`).
  - `motocross`: Mid-air pitch control authority (`fourth_upgrade_type = 2`, `air_pitch_torque = 16.0` N·m).
  - `monster_truck`: 4WD torque distribution ratio (`fourth_upgrade_type = 0`).
  - `race_car`: Aerodynamic downforce scaling quadratically with forward speed (`fourth_upgrade_type = 3`, $F_y \le 150.0$ N).

### 2. Physical Driver Head & Neck Ragdoll (T-015)
- Dynamic driver head ($r=0.24$ m, `CAT_DRIVER`, mass `0.0905` kg) connected via Box2D `b2RevoluteJoint`.
- Angular limits strictly enforced between `[-0.60, +0.60]` rad (`[-34.4°, +34.4°]`).
- Proportional-Derivative (PD) neck muscle spring motor keeps head upright during normal driving.
- Weakened limp ragdoll torque ($0.5$ N·m) upon driver impact.
- Head collision with solid terrain (`CAT_TERRAIN`, `!isSensor`) triggers **Driver Down** game-over sequence while allowing non-fatal collection of coin and fuel sensors (`CAT_ITEM`).

### 3. Full Trick Bonus Suite & HUD Feedback (T-113)
- **Air Time Bonus:** Accumulates when airborne; awards 250 coins/sec upon landing when airborne $\ge 1.0$s with `"AIR TIME!"` banner.
- **Wheelie Bonus:** Awards 500 coins when rear wheel is grounded and front wheel is airborne $\ge 1.5$s with `"WHEELIE!"` banner.
- **Flips:** Preserved detection for 360° `BACKFLIP` and `FRONTFLIP` rotations.

### 4. High-Integrity Binary Save System (T-030, T-031, H4, H5)
- Header magic `HCRS`, payload version 1, natural 4-byte MIPS Allegrex memory alignment (no bus errors).
- End-to-end IEEE 802.3 CRC32 integrity verification.
- Safe atomic persistence with automatic backup (`.bak`) recovery on partial/interrupted write.
- Strict canonical FNV-1a hash consistency across saves and garage data.

### 5. PSP Hardware Optimizations (T-111, T-116, T-119)
- Swizzled native `.tex` textures bypass costly runtime PNG decoding, slashing heap pressure by $>2.0$ MB and eliminating frame drops.
- Dynamic heap management using `PSP_HEAP_SIZE_KB(-1024)` reserves maximum available memory on PSP-1000/2000/3000.
- Amortized terrain chunk generation caps physics computation spikes to under 2 ms.

### 6. Verification & Quality Assurance
- **14 Continuous Integration Test Suites:** 100% automated pass rate across physics, suspension, air control, item sensors, terrain determinism, save integrity, garage economy, vehicle definitions, and trick bonuses.
- **Deterministic Heights:** Cross-platform heightfield test suite ensures identical terrain geometry on both x86_64 PC and MIPS Allegrex PSP.

---

## Distribution Packages & Checksums

| Package Archive | Platform | Size | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| `hcr-psp-v0.1.0-beta.2.zip` | Sony PSP (CFW / PPSSPP) | 289,918 bytes | `645c2249fd042883779d625d4c5e14491c2a9780d40d536321a77018d339596a` |
| `hcr-pc-v0.1.0-beta.2-linux.tar.gz` | Linux PC (x86_64, SDL2) | 257,133 bytes | `cbbb58c850255e769813c67d1a72b80e2a54026bc0715287c1da21d4ee56e38a` |

---

## Installation & Quick Start

### Sony PlayStation Portable (PSP) / PPSSPP
1. Extract `hcr-psp-v0.1.0-beta.2.zip` to your computer.
2. Copy the `PSP` folder to the root of your Memory Stick (`ms0:/PSP/GAME/HCR/EBOOT.PBP`).
3. Extract assets from your legally owned APK using:
   ```bash
   python3 tools/convert_assets.py --apk path/to/hcr-1.30.0.apk --out ms0:/PSP/GAME/HCR/assets/
   ```
4. Launch from the PSP XMB under Game $\to$ Memory Stick.

### Linux PC
1. Extract `hcr-pc-v0.1.0-beta.2-linux.tar.gz`.
2. Convert assets into the `assets/` directory next to the executable.
3. Install SDL2 runtime:
   ```bash
   sudo apt-get install -y libsdl2-2.0-0 libgl1-mesa-glx
   ```
4. Run `./hcr_pc`.
