import os
import sys
import shutil

src_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
dist_hcr = os.path.join(src_root, "dist", "PSP", "GAME", "HCR")

targets = [dist_hcr]
if len(sys.argv) > 1:
    targets.extend(sys.argv[1:])

for t in targets:
    if os.path.exists(t):
        shutil.rmtree(t)
    os.makedirs(t, exist_ok=True)

    # Copy EBOOT.PBP
    shutil.copy2(os.path.join(src_root, "EBOOT.PBP"), os.path.join(t, "EBOOT.PBP"))

    # Copy assets
    shutil.copytree(os.path.join(src_root, "assets"), os.path.join(t, "assets"))

    # Copy data configs
    shutil.copytree(os.path.join(src_root, "data"), os.path.join(t, "data"))

    print(f"Packaged successfully to: {t}")

print("All packaging done!")

