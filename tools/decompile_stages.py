import struct

def f32(val):
    return struct.unpack('<f', struct.pack('<I', val))[0]

def d64(lo, hi):
    return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

with open("/mnt/c/Projects/hcr-psp/tools/countryside.asm") as f:
    lines = f.readlines()

print("=== countryside.asm lines 120-end ===")
for i in range(120, len(lines)):
    print(lines[i].strip())
