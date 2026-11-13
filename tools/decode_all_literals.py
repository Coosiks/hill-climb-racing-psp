import struct, re

def f32(val):
    return struct.unpack('<f', struct.pack('<I', val))[0]

def d64(lo, hi):
    return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

def get_pool(filename):
    with open(filename) as f:
        lines = f.readlines()
    pool = {}
    for line in lines:
        m = re.match(r'^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)$', line)
        if m:
            addr = int(m.group(1), 16)
            raw = int(m.group(2), 16)
            pool[addr] = raw
    return lines, pool

def decode_literals(filename):
    lines, pool = get_pool(filename)
    print(f"=== Literal pool for {filename} ===")
    for addr, raw in pool.items():
        # check if it's referenced by ldr [pc, #...]
        f = f32(raw)
        # also check if raw could be part of double
        print(f"  {hex(addr)}: 0x{raw:08x}  float={f}")

for name in ["countryside", "desert", "arctic", "moon"]:
    decode_literals(f"/mnt/c/Projects/hcr-psp/tools/{name}.asm")

