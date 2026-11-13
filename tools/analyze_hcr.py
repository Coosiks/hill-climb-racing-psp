import os
import subprocess
import re
import struct

so_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../original_apk/lib/armeabi/libgame.so')

disasm = subprocess.check_output(['arm-linux-gnueabi-objdump', '-d', '-C', so_path]).decode(errors='ignore')
lines = disasm.splitlines()

# Function to extract and display disassembly of a function
def inspect_func(pattern, max_lines=120):
    print(f"\n====================================")
    print(f"=== Function: {pattern} ===")
    print(f"====================================")
    found = False
    count = 0
    for line in lines:
        if pattern in line and '>:' in line:
            found = True
        if found:
            print(line)
            count += 1
            if count >= max_lines:
                break
            if count > 1 and '>:' in line and not pattern in line:
                break

# Let's inspect Stage functions for heights and friction
inspect_func('StageCountrySide::getHeightAtX', 60)
inspect_func('StageCountrySide::getFriction', 20)
inspect_func('StageMoon::getFriction', 20)

# Inspect PlayerCar::accelerate and decelerate
inspect_func('PlayerCar::accelerate', 60)
inspect_func('PlayerCar::decelerate', 60)

# Inspect PlayerCar::create
inspect_func('PlayerCar::create(', 150)

# Inspect Terrain::addSegment
inspect_func('Terrain::addSegment', 60)
inspect_func('Terrain::create(', 80)
