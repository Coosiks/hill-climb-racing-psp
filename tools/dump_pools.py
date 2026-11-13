import os, struct

def f32(val):
    return struct.unpack('<f', struct.pack('<I', val))[0]

def d64(lo, hi):
    return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

tools_dir = os.path.dirname(os.path.abspath(__file__))
for name in ["desert", "arctic", "moon"]:
    path = os.path.join(tools_dir, f"{name}.asm")
    with open(path) as f:
        lines = f.readlines()
    print(f"=== {name.upper()} (total lines: {len(lines)}) ===")
    # Print the last 40 lines (constants pool)
    for l in lines[-40:]:
        print(l.strip())

