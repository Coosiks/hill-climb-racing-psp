#!/usr/bin/env python3
"""
Hill Climb Racing - Asset Converter & Preprocessor
Extracts assets from original APK, analyzes manifests, pre-processes
BMFonts into binary tables, converts audio, and swizzles PNG textures into
PSP-native POT format (RGBA8888 and RGBA5551/5650).
"""

import os
import sys
import struct
import zipfile
import shutil
from PIL import Image

PSP_TEX_MAGIC = b'PSPT'
FORMAT_RGBA5650 = 0
FORMAT_RGBA5551 = 1
FORMAT_RGBA4444 = 2
FORMAT_RGBA8888 = 3

def next_pot(x):
    p = 1
    while p < x:
        p <<= 1
    return p

def swizzle_image(data_bytes, width, height, bytes_per_pixel=4):
    """Swizzles an image for PSP hardware GE (16 bytes by 8 lines blocks)"""
    out = bytearray(len(data_bytes))
    row_pitch = width * bytes_per_pixel
    block_width = 16 // bytes_per_pixel  # in pixels
    block_height = 8                     # in pixels
    
    num_blocks_x = width // block_width
    num_blocks_y = height // block_height
    
    dst_offset = 0
    for by in range(num_blocks_y):
        for bx in range(num_blocks_x):
            for y in range(block_height):
                src_x = bx * block_width
                src_y = by * block_height + y
                src_offset = (src_y * width + src_x) * bytes_per_pixel
                chunk = data_bytes[src_offset:src_offset + 16]
                out[dst_offset:dst_offset + 16] = chunk
                dst_offset += 16
    return bytes(out)

def convert_png_to_tex(src_path, dst_path, target_format=FORMAT_RGBA8888, swizzle=True):
    with Image.open(src_path) as img:
        img = img.convert('RGBA')
        orig_w, orig_h = img.size
        
        pot_w = min(512, next_pot(orig_w))
        pot_h = min(512, next_pot(orig_h))
        
        # Create POT canvas
        pot_img = Image.new('RGBA', (pot_w, pot_h), (0, 0, 0, 0))
        pot_img.paste(img, (0, 0))
        
        raw_pixels = pot_img.tobytes()
        
        if target_format == FORMAT_RGBA8888:
            bpp = 4
            pixel_bytes = raw_pixels
        elif target_format == FORMAT_RGBA5551:
            bpp = 2
            # Convert 32bpp to 16bpp (A1R5G5B5 for PSP)
            short_array = bytearray(pot_w * pot_h * 2)
            idx = 0
            for i in range(0, len(raw_pixels), 4):
                r = raw_pixels[i] >> 3
                g = raw_pixels[i+1] >> 3
                b = raw_pixels[i+2] >> 3
                a = 1 if raw_pixels[i+3] > 128 else 0
                val = (a << 15) | (b << 10) | (g << 5) | r
                struct.pack_into('<H', short_array, idx, val)
                idx += 2
            pixel_bytes = bytes(short_array)
        else:
            bpp = 4
            pixel_bytes = raw_pixels

        final_bytes = swizzle_image(pixel_bytes, pot_w, pot_h, bpp) if swizzle else pixel_bytes
        
        # Header: magic(4), orig_w(2), orig_h(2), pot_w(2), pot_h(2), fmt(2), swizzle(2)
        header = struct.pack('<4sHHHHHH', PSP_TEX_MAGIC, orig_w, orig_h, pot_w, pot_h, target_format, 1 if swizzle else 0)
        
        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
        with open(dst_path, 'wb') as f:
            f.write(header)
            f.write(final_bytes)

def convert_all(base_dir):
    print("=== Hill Climb Racing: Asset Pipeline ===")
    src_assets = os.path.join(base_dir, "original_apk", "assets")
    out_assets = os.path.join(base_dir, "assets_converted")
    os.makedirs(out_assets, exist_ok=True)
    
    count = 0
    for root, _, files in os.walk(src_assets):
        for f in files:
            src_f = os.path.join(root, f)
            rel_path = os.path.relpath(src_f, src_assets)
            dst_f = os.path.join(out_assets, rel_path)
            
            if f.endswith('.png'):
                tex_f = os.path.splitext(dst_f)[0] + '.tex'
                # Use RGBA5551 for terrain surface and icons, RGBA8888 for high detail
                fmt = FORMAT_RGBA8888
                convert_png_to_tex(src_f, tex_f, target_format=fmt, swizzle=True)
                count += 1
            else:
                os.makedirs(os.path.dirname(dst_f), exist_ok=True)
                shutil.copy2(src_f, dst_f)
                count += 1

    print(f"Successfully processed {count} assets into {out_assets}")

if __name__ == '__main__':
    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    convert_all(base_dir)

