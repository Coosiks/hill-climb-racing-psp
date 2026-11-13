import re, struct

def f32(val):
    val = val & 0xffffffff
    return struct.unpack('<f', struct.pack('<I', val))[0]

def d64(lo, hi):
    lo = lo & 0xffffffff
    hi = hi & 0xffffffff
    return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

def check_doubles(name):
    filename = f"/mnt/c/Projects/hcr-psp/tools/{name}.asm"
    with open(filename) as f:
        lines = f.readlines()
    words = {}
    for line in lines:
        m = re.match(r'^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)$', line)
        if m:
            addr = int(m.group(1), 16)
            raw = int(m.group(2), 16)
            words[addr] = raw

    print(f"\n==================== {name.upper()} DOUBLES ====================")
    for i, line in enumerate(lines):
        if 'dmul' in line:
            # print preceding lines
            print(f"[{name.upper()}] DMUL at {line.strip()[:6]}:")
            for j in range(max(0, i-8), i+1):
                l = lines[j].strip()
                # If there's an ldr, print decoded
                m_ldr = re.search(r'\[pc,\s*#(\d+)\]', l)
                if m_ldr:
                    target = int(l.split(':')[0], 16) + 8 + int(m_ldr.group(1))
                    val = words.get(target, 0)
                    print(f"   {l}  --> pool[{hex(target)}] = 0x{val:08x} (f32: {f32(val)})")
                else:
                    print(f"   {l}")

for s in ["desert", "arctic", "moon"]:
    check_doubles(s)

