import struct

def f32(val):
    return struct.unpack('<f', struct.pack('<I', val))[0]

def d64(lo, hi):
    return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

stages = {
    "CountrySide": [
        ("t_scale", f32(0x3dcccccd)), # 0.1
        ("c1", f32(0x3ac49ba6)),      # 0.0015
        ("d_c1", d64(0xa0000000, 0x3fc99999)), # 0.2
        ("c2", f32(0x3e2e147b)),      # 0.17
        ("c3", f32(0x3a449ba6)),      # 0.00075
        ("c4", f32(0x3e19999a)),      # 0.15
        ("c5", f32(0x3dc2f838)),      # 0.0952
        ("c6", f32(0x3b343958)),      # 0.00275
        ("c7", f32(0x3cfee288)),      # 0.03111388
        ("c8", f32(0x3d088889)),      # 0.03333334
        ("amp_rate", f32(0x3a03126f)),# 0.0005
    ],
    "Desert": [
        ("d5594", f32(0x3e1c18fa)),
        ("d5598", f32(0x3eaaaaab)),
        ("d559c", f32(0x3e333333)),
        ("d55a0", f32(0x3ac49ba6)),
        ("d55a4", f32(0x3fd66666)),
        ("d55a8", f32(0x3fd33333)),
        ("d55ac", f32(0x3d52f1aa)),
        ("d55b0", f32(0x3de147ae)),
        ("d55b4", f32(0x3a449ba6)),
        ("d55b8", f32(0x3c7765fe)),
        ("d55bc", f32(0x3e072b02)),
        ("d55c0", f32(0x3b343958)),
        ("d55c4", f32(0x3c7f70f4)),
        ("d55c8", f32(0x3d088889)),
        ("d55cc", f32(0x3ae56042)),
        ("d55d0", f32(0x3a03126f)),
    ],
    "Arctic": [
        ("d5138", f32(0x3dcccccd)),
        ("d513c", f32(0x3ea49ba6)),
        ("d5140", f32(0x3ac49ba6)),
        ("d5144", f32(0x3fb99999)),
        ("d5148", f32(0x3e28f5c3)),
        ("d514c", f32(0x3e3851ec)),
        ("d5150", f32(0x3abe0ded)),
        ("d5154", f32(0x3deb851f)),
        ("d5158", f32(0x3dc2f838)),
        ("d515c", f32(0x3b343958)),
        ("d5160", f32(0x3cfee288)),
        ("d5164", f32(0x3dba2e8c)),
        ("d5168", f32(0x3a23d70a)),
    ],
    "Moon": [
        ("d4d40", f32(0x3ac49ba6)),
        ("d4d44", f32(0x3fc99999)),
        ("d4d48", f32(0x3ee66666)),
        ("d4d4c", f32(0x3e0f5c29)),
        ("d4d50", f32(0x3a449ba6)),
        ("d4d54", f32(0x3dc28f5c)),
        ("d4d58", f32(0x3d9a0275)),
        ("d4d5c", f32(0x3b343958)),
        ("d4d60", f32(0x3d41c184)),
        ("d4d64", f32(0x3d088889)),
        ("d4d68", f32(0x3a2ec33e)),
    ]
}

for name, consts in stages.items():
    print(f"=== {name} ===")
    for label, val in consts:
        print(f"  {label}: {val}")

