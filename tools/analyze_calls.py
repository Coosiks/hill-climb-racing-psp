import re

def analyze(filename):
    with open(filename) as f:
        lines = f.readlines()
    
    calls = []
    for line in lines:
        if "bl\t" in line or "bl " in line:
            # extract call target
            m = re.search(r'bl\s+([0-9a-f]+)\s+<([^>]+)>', line)
            if m:
                calls.append((line.split(':')[0].strip(), m.group(2)))
    return calls

for name in ["countryside", "desert", "arctic", "moon"]:
    c = analyze(f"/mnt/c/Projects/hcr-psp/tools/{name}.asm")
    print(f"=== {name.upper()} calls ({len(c)}) ===")
    for addr, target in c:
        print(f"  {addr}: {target}")

