import struct

def f32(val):
    return struct.unpack('<f', struct.pack('<I', val))[0]

def d64(lo, hi):
    return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

for name in ["desert", "arctic", "moon"]:
    path = f"/mnt/c/Projects/hcr-psp/tools/{name}.asm"
    with open(path) as f:
        lines = f.readlines()
    print(f"=== {name.upper()} (total lines: {len(lines)}) ===")
    # Print the last 40 lines (constants pool)
    for l in lines[-40:]:
        print(l.strip())

