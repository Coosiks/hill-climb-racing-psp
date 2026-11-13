import re, struct

def f32(val):
    return struct.unpack('<f', struct.pack('<I', val))[0]

def d64(lo, hi):
    return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

def decompile_stage(asm_path):
    with open(asm_path) as f:
        lines = f.readlines()
    
    # Parse literals pool
    pool = {}
    for line in lines:
        m = re.match(r'^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)$', line)
        if m:
            addr = int(m.group(1), 16)
            raw = int(m.group(2), 16)
            pool[addr] = raw

    # Trace execution
    regs = {}
    for r in ['r0','r1','r2','r3','r4','r5','r6','r7','r8','r9','sl','fp','ip']:
        regs[r] = None
    
    # Initial state: r0 is 'this', r1 is 'x'
    regs['r0'] = "this"
    regs['r1'] = "x"
    
    # We will log symbolic operations
    code_log = []
    var_counter = 0
    def new_var(prefix="v"):
        nonlocal var_counter
        var_counter += 1
        return f"{prefix}{var_counter}"

    i = 0
    while i < len(lines):
        line = lines[i].strip()
        i += 1
        m = re.match(r'^([0-9a-f]+):\s+([0-9a-f]{8})\s+(\S+)\s*(.*)$', line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        raw = int(m.group(2), 16)
        mnemonic = m.group(3)
        args_str = m.group(4)
        # remove comments
        if '@' in args_str:
            args_str = args_str.split('@')[0].strip()
        args = [a.strip() for a in args_str.split(',') if a.strip()]

        if mnemonic == 'push':
            continue
        elif mnemonic == 'pop':
            # return
            code_log.append(f"return {regs['r0']};")
            break
        elif mnemonic == 'mov':
            dst = args[0]
            src = args[1]
            if src.startswith('#'):
                val = int(src[1:], 0)
                regs[dst] = f32(val)
            else:
                regs[dst] = regs[src]
        elif mnemonic == 'moveq':
            dst = args[0]
            src = args[1]
            # condition
            if src.startswith('#'):
                val = int(src[1:], 0)
                regs[dst] = f32(val)
            else:
                regs[dst] = regs[src]
        elif mnemonic == 'ldr':
            dst = args[0]
            # check pc relative: [pc, #offset]
            m_ldr = re.search(r'\[pc,\s*#(\d+)\]', args_str)
            if m_ldr:
                offset = int(m_ldr.group(1))
                target = addr + 8 + offset
                if target in pool:
                    regs[dst] = f32(pool[target])
                else:
                    regs[dst] = f"pool_{hex(target)}"
        elif mnemonic == 'add':
            dst = args[0]
            op1 = args[1]
            op2 = args[2]
            v1 = regs[op1] if op1 in regs else None
            if op2.startswith('#'):
                v2_int = int(op2[1:], 0)
                # if op1 is a float or int
                if isinstance(v1, float):
                    # bitwise add or float add?
                    # in ARM, mov r1, #... ; add r1, r1, #... is constructing a constant
                    pass
            regs[dst] = f"({regs[op1]} + {regs[op2] if op2 in regs else op2})"
        elif mnemonic == 'bl':
            func = args[0]
            target_name = ""
            if '<' in line and '>' in line:
                target_name = line.split('<')[1].split('>')[0]
            
            if '__aeabi_fmul' in target_name:
                v = new_var("f")
                code_log.append(f"float {v} = {regs['r0']} * {regs['r1']};")
                regs['r0'] = v
            elif '__addsf3' in target_name:
                v = new_var("f")
                code_log.append(f"float {v} = {regs['r0']} + {regs['r1']};")
                regs['r0'] = v
            elif '__aeabi_f2d' in target_name:
                # r0 becomes (double)r0 in r0:r1
                v = new_var("d")
                code_log.append(f"double {v} = (double)({regs['r0']});")
                regs['r0'] = f"LO({v})"
                regs['r1'] = f"HI({v})"
                regs[v] = v
            elif '__aeabi_d2f' in target_name:
                v = new_var("f")
                code_log.append(f"float {v} = (float)({regs['r0']});")
                regs['r0'] = v
            elif '__aeabi_dmul' in target_name:
                v = new_var("d")
                code_log.append(f"double {v} = {regs['r0']} * {regs['r2']};")
                regs['r0'] = v
            elif '__adddf3' in target_name:
                v = new_var("d")
                code_log.append(f"double {v} = {regs['r0']} + {regs['r2']};")
                regs['r0'] = v
            elif 'cos' in target_name:
                v = new_var("d_cos")
                code_log.append(f"double {v} = cos({regs['r0']});")
                regs['r0'] = v
            elif 'sin' in target_name:
                v = new_var("d_sin")
                code_log.append(f"double {v} = sin({regs['r0']});")
                regs['r0'] = v
            elif '__aeabi_fcmpge' in target_name or '__aeabi_fcmple' in target_name:
                pass

    return code_log

print("Decompiler script ready")

