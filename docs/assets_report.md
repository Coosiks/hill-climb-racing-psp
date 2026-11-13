# Assets Inventory & Manifest Report: Hill Climb Racing (v1.0.5 / v1.18.0)

**Date:** November 2026  
**Source:** `original_apk/assets/`  
**Target:** PSP-1000/2000/3000 & PC

---

## 1. Overview & Budget Analysis

The APK contains 86 asset files across textures, fonts, audio, shaders, and configuration scripts. 
On the PSP, all assets must fit into a strict budget:
- **Total User RAM:** 24 MB reserved for game heap (out of 32 MB total on PSP-1000).
- **VRAM Budget:** 2 MB total eDRAM. Framebuffers (draw buffer 512x272x32b + display buffer 512x272x32b + depth buffer 512x272x16b) occupy 1.36 MB, leaving **~650 KB VRAM** for active textures.
- **PSP GPU Texture Limitation:** Maximum texture size is 512×512. Textures must have power-of-two dimensions (or padded up to POT) and must be swizzled for cache efficiency.

---

## 2. Graphic Assets Inventory

### 2.1 Vehicle & Character Sprites
| Filename | Native Size | POT Size | Channels | PSP VRAM (RGBA8888) | Usage |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Car.png` | 256×128 | 256×128 | RGBA (32b) | 131,072 B | Red Jeep / Player Car chassis |
| `Tire.png` | 64×64 | 64×64 | RGBA (32b) | 16,384 B | Wheels (front & rear) |
| `driver-body.png` | 128×64 | 128×64 | RGBA (32b) | 32,768 B | Driver torso & arms (attached to chassis) |
| `driver-head.png` | 64×64 | 64×64 | RGBA (32b) | 16,384 B | Driver head (revolute joint) |
| `antenna.png` | 8×16 | 8×16 | RGBA (32b) | 512 B | Springy vehicle antenna |
| `BridgeWoodBlock.png` | 64×32 | 64×32 | RGBA (32b) | 8,192 B | Bridge plank segments |

*Total Active Vehicle VRAM:* **~205 KB**

### 2.2 Terrain Textures (High-Res vs Low-Res)
The original game included both high-res (512x512) and low-res (128x128) ground textures:
| Stage | High-Res Texture | Native Size | Low-Res Texture | Native Size | Target PSP Format |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Countryside** | `terrain-ground.png` | 512×512 | `lowres-terrain-ground.png` | 128×128 | 128×128 RGBA5650 (32 KB swizzled) |
| | `terrain-surface.png` | 512×32 | `lowres-terrain-surface.png` | 256×16 | 256×16 RGBA5551 (8 KB swizzled) |
| **Desert** | `desert-ground.png` | 512×512 | `lowres-desert-ground.png` | 128×128 | 128×128 RGBA5650 (32 KB swizzled) |
| | `desert-surface.png` | 512×32 | `lowres-desert-surface.png` | 256×16 | 256×16 RGBA5551 (8 KB swizzled) |
| **Arctic** | `arctic-ground.png` | 512×512 | `lowres-arctic-ground.png` | 128×128 | 128×128 RGBA5650 (32 KB swizzled) |
| | `arctic-surface.png` | 512×32 | `lowres-arctic-surface.png` | 256×16 | 256×16 RGBA5551 (8 KB swizzled) |
| **Moon** | `moon-ground.png` | 512×512 | `lowres-moon-ground.png` | 128×128 | 128×128 RGBA5650 (32 KB swizzled) |
| | `moon-surface.png` | 512×32 | `lowres-moon-surface.png` | 256×16 | 256×16 RGBA5551 (8 KB swizzled) |

*Strategy:* On PSP (480×272 screen), repeating a 128×128 texture offers identical visual fidelity while reducing texture bandwidth by **16×** (32 KB vs 512 KB), leaving ample VRAM for other sprites.

### 2.3 Collectibles & Pickups
| Filename | Native Size | POT Size | PSP Memory | Usage |
| :--- | :--- | :--- | :--- | :--- |
| `Coin5.png` | 64×64 | 64×64 | 16 KB | Bronze Coin (value 5) |
| `Coin25.png` | 64×64 | 64×64 | 16 KB | Silver Coin (value 25) |
| `Coin100.png` | 64×64 | 64×64 | 16 KB | Gold Coin (value 100) |
| `Coin500.png` | 64×64 | 64×64 | 16 KB | Red Gem/Coin (value 500) |
| `fuel-canister.png` | 64×64 | 64×64 | 16 KB | Red Jerrycan (replenishes fuel tank) |

### 2.4 User Interface & HUD
| Filename | Native Size | Usage |
| :--- | :--- | :--- |
| `game-logo.png` | 205×147 | Title banner |
| `coin-icon.png` | 64×64 | Coin counter icon |
| `fuel-icon.png` | 64×64 | Fuel gauge icon |
| `distance-icon.png` | 64×64 | Distance meter icon |
| `fuel-warning.png` | 256×256 | Flashing red "LOW FUEL" alert |
| `lowres-pedal-gas-normal.png` / `-pressed.png` | 64×64 | Gas pedal HUD touch/button visual |
| `lowres-pedal-brake-normal.png` / `-pressed.png` | 64×64 | Brake pedal HUD touch/button visual |
| `MainMenuUpgradeEngine.png` / `-pressed.png` | 132×136 | Engine tuning card |
| `MainMenuUpgradeSuspension.png` / `-pressed.png` | 132×136 | Suspension tuning card |
| `MainMenuUpgradeTires.png` / `-pressed.png` | 132×136 | Tires tuning card |
| `MainMenuUpgradeAWD.png` / `-pressed.png` | 132×136 | 4WD transmission tuning card |
| `stage-icon-countryside.png` | 512×256 | Countryside stage card |
| `stage-icon-desert.png` | 512×256 | Desert stage card |
| `stage-icon-arctic.png` | 512×256 | Arctic stage card |
| `stage-icon-moon.png` | 512×256 | Moon stage card |
| `stage-icon-locked.png` | 512×256 | Locked stage padlock overlay |

---

## 3. Fonts Inventory

| Font Descriptor | Bitmap Texture | Glyph Count | Memory Footprint | Recommended Target |
| :--- | :--- | :--- | :--- | :--- |
| `fonts/gamefont.fnt` | `fonts/gamefont_0.png` (512×512) | ~180 glyphs | 1,048,576 B (RGBA8888) | PC Build |
| `fonts/gamefont_lowres.fnt` | `fonts/gamefont_lowres_0.png` (256×256) | ~180 glyphs | 131,072 B (RGBA4444) | **PSP Target** |

*Optimization:* On PSP, `gamefont_lowres` saves 87.5% memory compared to the high-res font and matches the 480×272 pixel grid perfectly.

---

## 4. Audio Assets Inventory

### 4.1 Sound Effects (SFX)
| Audio File | Duration | Original Format | Target Format (PSP) | Target Format (PC) | Priority |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `audio/engine.ogg` | 0.85 s loop | Ogg Vorbis (mono) | `sceSas` ADPCM loop (pitch 0.5–2.0) | SDL_Mixer chunk | Highest (Channel 0) |
| `audio/coin.ogg` | 0.32 s | Ogg Vorbis (stereo) | `sceSas` ADPCM voice | SDL_Mixer chunk | High (Channel 1) |
| `audio/refuel.ogg` | 0.45 s | Ogg Vorbis (stereo) | `sceSas` ADPCM voice | SDL_Mixer chunk | High (Channel 2) |
| `audio/crack.ogg` | 0.60 s | Ogg Vorbis (stereo) | `sceSas` ADPCM voice | SDL_Mixer chunk | High (Channel 3) |
| `audio/click.ogg` | 0.08 s | Ogg Vorbis (stereo) | `sceSas` ADPCM voice | SDL_Mixer chunk | Normal (Channel 4) |
| `audio/turbo.ogg` | 0.90 s | Ogg Vorbis (stereo) | `sceSas` ADPCM voice | SDL_Mixer chunk | Normal (Channel 5) |
| `audio/turbo-flutter.ogg` | 0.75 s | Ogg Vorbis (stereo) | `sceSas` ADPCM voice | SDL_Mixer chunk | Normal (Channel 6) |
| `audio/camera-shutter.ogg` | 0.50 s | Ogg Vorbis (stereo) | `sceSas` ADPCM voice | SDL_Mixer chunk | Low |
| `audio/purchase.ogg` | 0.70 s | Ogg Vorbis (stereo) | `sceSas` ADPCM voice | SDL_Mixer chunk | Normal |
| `audio/beep.ogg` | 0.20 s | Ogg Vorbis (stereo) | `sceSas` ADPCM voice | SDL_Mixer chunk | Low |

### 4.2 Background Music
| Music File | Size | Duration | Handling Strategy |
| :--- | :--- | :--- | :--- |
| `audio/bgmusic00.ogg` | 507 KB | ~1 min loop | Streamed double-buffered PCM/WAV via `sceAudio` (16 KB buffer chunk) |
| `audio/bgmusic01.ogg` | 490 KB | ~1 min loop | Streamed double-buffered PCM/WAV via `sceAudio` (16 KB buffer chunk) |

---

## 5. Particle Systems & Shaders

### 5.1 Particle XML Plists
- `dirt.plist`: Tire dust/mud particle system (`dirt.png` 64×64). Emitter settings: 200 max particles, lifespan 0.5s, gravity $(0, -30)$, speed 30.
- `snow.plist`: Snow particle system for Arctic stage (`snow.png` 64×64). Emitter settings: 200 max particles, gravity $(0, -10)$, soft downward drift.

*PSP Implementation:* Implemented as a static circular particle pool (max 128 active particles on PSP) rendered with batched quad points.

### 5.2 Shaders
- `shaders/sky.vsh` & `sky.fsh`: Simple textured pass-through. On PSP: Rendered using native fixed-function `sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB)`.
- `shaders/terrain.vsh` & `terrain.fsh`: Modulates surface ground texture based on vertical penetration depth `v_texCoord2.y`:
  $$mul = (0.7 + \text{clamp}(y \cdot 12, 0, 1) \cdot 0.4) \cdot (1.6 - \text{clamp}(y \cdot 0.5, 0, 1) \cdot 0.8)$$
  *PSP Implementation:* Since PSP's GPU lacks programmable pixel shaders, this depth gradient is approximated via vertex colors assigned to the terrain triangle-strip vertices.

---

## 6. Asset Conversion Pipeline Design (`tools/convert_assets.py`)

The offline asset converter script will:
1. **Unpack & Verify:** Check input APK / raw assets folder.
2. **Texture Swizzling:**
   - Pad textures to next power-of-two dimensions (POT).
   - Convert images to 16-bit RGBA5551 or RGBA5650 where appropriate to save 50% memory, and RGBA8888 for high-detail sprites.
   - Rearrange linear pixel scanlines into PSP hardware 16×8-byte swizzled tiles.
   - Output binary `.tex` format with a 16-byte header: `[magic (4B), width (2B), height (2B), pot_w (2B), pot_h (2B), psm (2B), swizzled (2B)]`.
3. **Audio Pre-processing:**
   - Transcode OGG sound effects to uncompressed 16-bit 22050 Hz / 44100 Hz mono PCM or VAG ADPCM format for instant zero-latency playback via `sceSas`.
   - Transcode BGM loops into streaming WAV/ADPCM chunks.
4. **Font Compilation:**
   - Parse `.fnt` files into compact binary glyph tables (`.fntb`) containing direct UV and metrics lookup arrays, eliminating runtime XML/text parsing.

