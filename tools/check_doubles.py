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

    print(f"=== {name.upper()} DOUBLES ===")
    for i, line in enumerate(lines):
        if 'dmul' in line or 'adddf3' in line:
            # check preceding 6 instructions for r2 / r3 setup
            window = lines[max(0, i-6):i+1]
            print("Operation:", line.strip())
            for w in window[:-1]:
                print("   ", w.strip())
            print()

check_doubles("countryside")

