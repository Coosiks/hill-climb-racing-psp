#!/usr/bin/env python3
import os
import sys
import zipfile
import tarfile
import hashlib
import argparse

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
RELEASE_DIR = os.path.join(ROOT_DIR, "release")

os.makedirs(RELEASE_DIR, exist_ok=True)

README_CONTENT_TEMPLATE = """===================================================================
Hill Climb Racing - PSP Fan Port ({version})
===================================================================

IMPORTANT LEGAL DISCLAIMER:
This is a non-commercial, fan-made RE-assisted native reimplementation of
Hill Climb Racing for the PlayStation Portable (PSP) and PC.
Hill Climb Racing is a registered trademark of Fingersoft Ltd.
This package contains ZERO proprietary or copyrighted assets (textures,
sprites, audio, or game data).

All rights to the original game, vehicle designs, characters, sound effects,
and trademarks belong exclusively to Fingersoft Ltd.

===================================================================
1. SONY PLAYSTATION PORTABLE (PSP) INSTALLATION
===================================================================
Requirements:
- PSP-1000, PSP-2000, PSP-3000, or PSP Street (E1000) running Custom Firmware
  (e.g., 6.60/6.61 PRO-C, ME, or ARK-4), OR the PPSSPP emulator (v1.17+).

Installation Steps:
1. Copy the 'PSP' directory from this archive directly to the root of your
   Memory Stick (ms0:/).
   The path should be: ms0:/PSP/GAME/HCR/EBOOT.PBP
2. Obtain your legally owned copy of Hill Climb Racing for Android (v1.30.0 or v1.0.5 APK).
3. Use the asset converter script from the GitHub repository:
   https://github.com/Coosiks/hill-climb-racing-psp
   Run:
     python3 tools/convert_assets.py --apk path/to/hcr.apk --out ms0:/PSP/GAME/HCR/assets/
4. Launch "Hill Climb Racing" from the PSP Game -> Memory Stick menu.

Note: If launched without assets, the game will safely display an on-screen prompt
explaining that asset conversion is required.

===================================================================
2. CONTROLS (PSP)
===================================================================
- Gas / Accelerate / Tilt Up (Air, Backflip):   Cross [X] or R-Trigger
- Brake / Reverse / Tilt Down (Air, Frontflip): Square [] or L-Trigger
- Start Race / Select Option:                   Cross [X] or Start
- Garage Navigation (Vehicles/Stages/Upgrades): L / R Triggers
- Upgrades (Engine/Suspension/Tires/4th Slot):  Triangle / Square / Circle / Down
- Pause / In-Game Menu:                         Start
- Reset / Restart Stage:                        Select
- Exit / Back:                                  Circle (O)

===================================================================
3. PC (LINUX) INSTALLATION
===================================================================
1. Extract the tar.gz archive to a directory.
2. Ensure SDL2 and OpenGL are installed:
   Ubuntu/Debian: sudo apt-get install -y libsdl2-2.0-0 libgl1-mesa-glx
3. Place converted assets in the 'assets/' folder next to the 'hcr_pc' binary.
4. Run:
   ./hcr_pc

Controls (PC):
- Gas:               D / Right Arrow
- Brake:             A / Left Arrow
- Pause / Menu:      Escape / Enter
- Reset:             R
- Upgrades (1-4):    1, 2, 3, 4

===================================================================
4. PROJECT REPOSITORY & BUG REPORTS
===================================================================
GitHub:   https://github.com/Coosiks/hill-climb-racing-psp
Status:   docs/STATUS.md
Roadmap:  docs/ROADMAP.md
===================================================================
"""

def get_sha256(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser(description="Package PSP and PC release distributions.")
    parser.add_argument("--version", default="v0.1.0-beta.2", help="Release version tag (e.g. v0.1.0-beta.2)")
    args = parser.parse_args()

    ver = args.version
    readme_content = README_CONTENT_TEMPLATE.format(version=ver)

    # Locate EBOOT.PBP
    eboot_candidates = [
        os.path.join(ROOT_DIR, "EBOOT.PBP"),
        os.path.join(ROOT_DIR, "dist", "PSP", "GAME", "HCR", "EBOOT.PBP")
    ]
    eboot_src = None
    for cand in eboot_candidates:
        if os.path.exists(cand):
            eboot_src = cand
            break
    if not eboot_src:
        sys.exit("Error: EBOOT.PBP not found in root or dist/")

    # 1. Package PSP zip
    psp_zip_filename = f"hcr-psp-{ver}.zip"
    psp_zip_path = os.path.join(RELEASE_DIR, psp_zip_filename)
    with zipfile.ZipFile(psp_zip_path, 'w', zipfile.ZIP_DEFLATED) as zf:
        zf.write(eboot_src, arcname="PSP/GAME/HCR/EBOOT.PBP")
        config_dir = os.path.join(ROOT_DIR, "config")
        for cfg in ["physics.cfg", "vehicles.cfg", "stages.cfg"]:
            cfg_path = os.path.join(config_dir, cfg)
            zf.write(cfg_path, arcname=f"PSP/GAME/HCR/config/{cfg}")
        zf.writestr("PSP/GAME/HCR/README.txt", readme_content)

    print(f"Created: {psp_zip_path} (size: {os.path.getsize(psp_zip_path)} bytes)")

    # 2. Package PC tar.gz
    hcr_pc_src = os.path.join(ROOT_DIR, "build-pc", "hcr_pc")
    if not os.path.exists(hcr_pc_src):
        sys.exit(f"Error: {hcr_pc_src} not found. Run CMake PC build first.")

    pc_tar_filename = f"hcr-pc-{ver}-linux.tar.gz"
    pc_tar_path = os.path.join(RELEASE_DIR, pc_tar_filename)
    with tarfile.open(pc_tar_path, "w:gz") as tar:
        tar.add(hcr_pc_src, arcname=f"hcr-pc-{ver}/hcr_pc")
        config_dir = os.path.join(ROOT_DIR, "config")
        for cfg in ["physics.cfg", "vehicles.cfg", "stages.cfg"]:
            cfg_path = os.path.join(config_dir, cfg)
            tar.add(cfg_path, arcname=f"hcr-pc-{ver}/config/{cfg}")
        
        import io
        readme_data = readme_content.encode("utf-8")
        ti = tarfile.TarInfo(name=f"hcr-pc-{ver}/README.txt")
        ti.size = len(readme_data)
        tar.addfile(ti, io.BytesIO(readme_data))

    print(f"Created: {pc_tar_path} (size: {os.path.getsize(pc_tar_path)} bytes)")

    # 3. Checksums
    checksums_path = os.path.join(RELEASE_DIR, "checksums.txt")
    with open(checksums_path, "w", encoding="utf-8") as f:
        for filename in [psp_zip_filename, pc_tar_filename]:
            fpath = os.path.join(RELEASE_DIR, filename)
            sha = get_sha256(fpath)
            size = os.path.getsize(fpath)
            f.write(f"{sha}  {filename}  ({size} bytes)\n")

    with open(checksums_path, "r", encoding="utf-8") as f:
        print("\nChecksums:\n" + f.read())

if __name__ == "__main__":
    main()
