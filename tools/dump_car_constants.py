import os, struct

# Let's read the literal pool of PlayerCar::create at d2c84 to d2cd0
so_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../original_apk/lib/armeabi/libgame.so')
with open(so_path, 'rb') as f:
    f.seek(0xd2c84)
    data = f.read(96)
    for i in range(0, len(data), 4):
        val = data[i:i+4]
        uint_val = struct.unpack('<I', val)[0]
        flt_val = struct.unpack('<f', val)[0]
        print(f'{0xd2c84 + i:08x}: hex=0x{uint_val:08x} uint={uint_val} float={flt_val}')

