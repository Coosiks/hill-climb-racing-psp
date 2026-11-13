import re, struct

def f32(val):
    val = val & 0xffffffff
    return struct.unpack('<f', struct.pack('<I', val))[0]

def d64(lo, hi):
    lo = lo & 0xffffffff
    hi = hi & 0xffffffff
    return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

def transpile(name):
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

    print(f"\n// ==================== STAGE: {name.upper()} ====================")
    
    # We will trace the register state
    regs = {f"r{i}": None for i in range(13)}
    regs['sl'] = None
    regs['fp'] = None
    regs['ip'] = None
    regs['r0'] = "this"
    regs['r1'] = "x"

    exprs = []
    var_id = 0
    def new_var(prefix="val"):
        nonlocal var_id
        var_id += 1
        return f"{prefix}_{var_id}"

    for line in lines:
        m = re.match(r'^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(\S+)\s*(.*)$', line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        raw = int(m.group(2), 16)
        mnemonic = m.group(3)
        args_str = m.group(4)
        if '@' in args_str:
            args_str = args_str.split('@')[0].strip()
        args = [a.strip() for a in args_str.split(',') if a.strip()]

        if mnemonic == 'mov':
            dst = args[0]
            src = args[1]
            if src.startswith('#'):
                v = int(src[1:], 0) & 0xffffffff
                regs[dst] = f"{f32(v)}f"
            else:
                regs[dst] = regs[src]
        elif mnemonic == 'moveq':
            dst = args[0]
            src = args[1]
            if src.startswith('#'):
                v = int(src[1:], 0) & 0xffffffff
                val_str = f"{f32(v)}f"
            else:
                val_str = regs[src]
            exprs.append(f"{regs[dst]} = (cmp_eq) ? {val_str} : {regs[dst]};")
        elif mnemonic == 'ldr':
            dst = args[0]
            m_ldr = re.search(r'\[pc,\s*#(\d+)\]', args_str)
            if m_ldr:
                offset = int(m_ldr.group(1))
                target = addr + 8 + offset
                val = words.get(target, 0)
                regs[dst] = f"{f32(val)}f"
        elif mnemonic == 'add':
            dst = args[0]
            op1 = args[1]
            op2 = args[2]
            if op2.startswith('#'):
                v2 = int(op2[1:], 0)
                regs[dst] = f"({regs[op1]} + {f32(v2)}f)"
            else:
                regs[dst] = f"({regs[op1]} + {regs[op2]})"
        elif mnemonic == 'bl':
            func_name = ""
            if '<' in line and '>' in line:
                func_name = line.split('<')[1].split('>')[0]
            
            if '__aeabi_fmul' in func_name:
                v = new_var("f_mul")
                exprs.append(f"float {v} = {regs['r0']} * {regs['r1']};")
                regs['r0'] = v
            elif '__addsf3' in func_name:
                v = new_var("f_add")
                exprs.append(f"float {v} = {regs['r0']} + {regs['r1']};")
                regs['r0'] = v
            elif '__aeabi_fsub' in func_name:
                v = new_var("f_sub")
                exprs.append(f"float {v} = {regs['r0']} - {regs['r1']};")
                regs['r0'] = v
            elif '__aeabi_f2d' in func_name:
                v = new_var("d_prom")
                exprs.append(f"double {v} = (double)({regs['r0']});")
                regs['r0'] = v
                regs['r1'] = f"{v}_hi"
            elif '__aeabi_d2f' in func_name:
                v = new_var("f_dem")
                exprs.append(f"float {v} = (float)({regs['r0']});")
                regs['r0'] = v
            elif '__aeabi_dmul' in func_name:
                v = new_var("d_mul")
                exprs.append(f"double {v} = {regs['r0']} * {regs['r2']};")
                regs['r0'] = v
            elif '__adddf3' in func_name:
                v = new_var("d_add")
                exprs.append(f"double {v} = {regs['r0']} + {regs['r2']};")
                regs['r0'] = v
            elif 'cos' in func_name:
                v = new_var("d_cos")
                exprs.append(f"double {v} = cos({regs['r0']});")
                regs['r0'] = v
            elif 'sin' in func_name:
                v = new_var("d_sin")
                exprs.append(f"double {v} = sin({regs['r0']});")
                regs['r0'] = v
            elif '__aeabi_fcmpge' in func_name:
                exprs.append(f"bool cmp_ge = ({regs['r0']} >= {regs['r1']});")
            elif '__aeabi_fcmple' in func_name:
                exprs.append(f"bool cmp_le = ({regs['r0']} <= {regs['r1']});")
        elif mnemonic == 'pop':
            exprs.append(f"return {regs['r0']};")
            break

    for e in exprs:
        print("  " + e)

for s in ["countryside", "desert", "arctic", "moon"]:
    transpile(s)
