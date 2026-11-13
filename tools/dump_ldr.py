import struct, re

def f32(val):
    return struct.unpack('<f', struct.pack('<I', val))[0]

def d64(lo, hi):
    return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

def get_pool(filename):
    with open(filename) as f:
        lines = f.readlines()
    words = {}
    for line in lines:
        m = re.match(r'^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)$', line)
        if m:
            addr = int(m.group(1), 16)
            raw = int(m.group(2), 16)
            words[addr] = raw
    return lines, words

for name in ["countryside", "desert", "arctic", "moon"]:
    lines, words = get_pool(f"/mnt/c/Projects/hcr-psp/tools/{name}.asm")
    print(f"\n==================== {name.upper()} ====================")
    for line in lines:
        m = re.search(r'([0-9a-f]+):\s+([0-9a-f]{8})\s+ldr\s+([a-z0-9]+),\s+\[pc,\s*#(\d+)\]', line)
        if m:
            addr = int(m.group(1), 16)
            reg = m.group(3)
            offset = int(m.group(4))
            target = addr + 8 + offset
            val = words.get(target, 0)
            f = f32(val)
            print(f"{hex(addr)}: ldr {reg}, [{hex(target)}] -> 0x{val:08x} (f32: {f})")

