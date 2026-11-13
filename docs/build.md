# Build Guide: Hill Climb Racing Native Port (PC & PSP)

This document provides complete instructions for building and running Hill Climb Racing on both Linux/PC and Sony PlayStation Portable.

---

## 1. Prerequisites & Toolchains

### 1.1 PC Build Environment
- **Operating System:** Linux (Ubuntu 22.04 / 24.04), macOS, or Windows (WSL2 / MSYS2)
- **Compiler:** GCC $\ge 9.0$ or Clang $\ge 11.0$ (supporting C++11)
- **Build System:** CMake $\ge 3.10$
- **Dependencies:**
  - SDL2 development libraries (`libsdl2-dev`)
  - OpenGL development libraries (`libgl-dev` or `libgl1-mesa-dev`)

```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y build-essential cmake libsdl2-dev libgl-dev python3 python3-pil
```

### 1.2 PSP Build Environment
- **PSP Toolchain:** `pspdev` toolchain (MIPS Allegrex compiler `psp-gcc` / `psp-g++`)
- **SDK Libraries:** PSPSDK with `pspgu`, `pspaudio`, `pspctrl`, `psppower`
- **Packaging Tools:** `mksfoex`, `pack-pbp`

Recommended installation via official Docker image or pre-built toolchain:
```bash
# Verify psp-config and psp-g++ are in your PATH
export PSPDEV=/opt/pspdev
export PATH=$PSPDEV/bin:$PATH
psp-config --pspsdk-path
```

---

## 2. Preparing Game Assets ("Bring Your Own APK")

To comply with intellectual property laws, no original Fingersoft assets are included in this repository.

1. Obtain a legally purchased APK of **Hill Climb Racing** (version 1.0.5 or 1.18.0).
2. Place the APK inside `original_apk/` or extract its `assets/` directory to `assets/`.
3. Run the asset preprocessor:
   ```bash
   python3 tools/convert_assets.py
   ```
   This script converts textures to hardware-swizzled power-of-two `.tex` files, formats audio, and builds font descriptors.

---

## 3. Building the PC Version

```bash
# Generate build configuration
cmake -B build-pc -S .

# Compile executable
cmake --build build-pc -j4

# Run the game
./build-pc/hcr_pc
```

**Keyboard Controls:**
- **Gas / Accelerate:** Right Arrow / D / R-trigger
- **Brake / Reverse:** Left Arrow / A / L-trigger
- **Select / Race:** Space / Enter
- **Back / Pause:** Escape / P
- **Upgrades:** Keys 1 (Engine), 2 (Suspension), 3 (Tires), 4 (4WD)

---

## 4. Building the PSP Version

```bash
# Using the automated build script
./build.sh

# Or directly with make
make -j4
```

This generates `EBOOT.PBP` (signed and stripped).

### 4.1 Installing onto Real PSP Hardware
1. Connect your PSP to PC via USB mode.
2. Navigate to your Memory Stick: `ms0:/PSP/GAME/`.
3. Create a folder named `HCR`: `ms0:/PSP/GAME/HCR/`.
4. Copy `EBOOT.PBP`, `config/`, and the converted `assets/` folder into `ms0:/PSP/GAME/HCR/`.
5. Launch the game from the PSP XMB menu under Game -> Memory Stick.

### 4.2 Running in PPSSPP Emulator
1. Open PPSSPP.
2. Select **Load...** and navigate to `c:/Projects/hcr-psp/EBOOT.PBP`.
3. Ensure the working directory contains `config/` and `assets/`.
