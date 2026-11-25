#!/usr/bin/env python3
"""
Hill Climb Racing - Asset Converter & Preprocessor (T-111)
Extracts assets from original APK or directory, converts textures into PSP-native POT
swizzled .tex format (RGBA8888 and RGBA5551), downscales oversized textures (>512),
and copies audio, fonts, and shaders to the canonical assets directory.
"""

import os
import sys
import struct
import zipfile
import shutil
import argparse
import io
import subprocess
import wave
import tempfile
import plistlib
import re
from PIL import Image

PSP_TEX_MAGIC = b'PSPT'
FORMAT_RGBA5650 = 0
FORMAT_RGBA5551 = 1
FORMAT_RGBA4444 = 2
FORMAT_RGBA8888 = 3

def parse_plist_rect(val):
    if not val:
        return []
    nums = [int(float(n)) for n in re.findall(r"[-+]?\d*\.?\d+", str(val))]
    return nums

def extract_frame_from_atlas(atlas_png_path, plist_path, target_frame_name, out_png_path):
    if not (os.path.isfile(atlas_png_path) and os.path.isfile(plist_path)):
        return False
    try:
        with open(plist_path, "rb") as f:
            pl = plistlib.load(f)
    except Exception as e:
        print(f"Warning: Failed to parse plist {plist_path}: {e}")
        return False

    frames = pl.get("frames", {})
    if not frames:
        return False

    chosen_frame_key = None
    if target_frame_name in frames:
        chosen_frame_key = target_frame_name
    else:
        for k in frames.keys():
            if target_frame_name.lower() in k.lower():
                chosen_frame_key = k
                break
    if not chosen_frame_key:
        return False

    finfo = frames[chosen_frame_key]
    frame_str = finfo.get("frame") or finfo.get("textureRect")
    rotated = finfo.get("rotated", False) or finfo.get("textureRotated", False)
    source_size_str = finfo.get("sourceSize") or finfo.get("spriteSourceSize")
    source_color_rect_str = finfo.get("sourceColorRect") or finfo.get("spriteColorRect")
    offset_str = finfo.get("offset") or finfo.get("spriteOffset")

    if not frame_str:
        return False

    f_nums = parse_plist_rect(frame_str)
    if len(f_nums) < 4:
        return False
    fx, fy, fw, fh = f_nums[0], f_nums[1], f_nums[2], f_nums[3]

    try:
        atlas_img = Image.open(atlas_png_path).convert("RGBA")
    except Exception as e:
        print(f"Warning: Failed to open atlas image {atlas_png_path}: {e}")
        return False

    if rotated:
        crop_box = (fx, fy, fx + fh, fy + fw)
    else:
        crop_box = (fx, fy, fx + fw, fy + fh)

    cropped = atlas_img.crop(crop_box)
    if rotated:
        cropped = cropped.transpose(Image.ROTATE_270)

    final_img = cropped
    if source_size_str:
        ss_nums = parse_plist_rect(source_size_str)
        if len(ss_nums) >= 2:
            sw, sh = ss_nums[0], ss_nums[1]
            if sw > 0 and sh > 0:
                final_img = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
                paste_x, paste_y = 0, 0
                if source_color_rect_str:
                    cr_nums = parse_plist_rect(source_color_rect_str)
                    if len(cr_nums) >= 4:
                        paste_x, paste_y = cr_nums[0], cr_nums[1]
                elif offset_str:
                    off_nums = parse_plist_rect(offset_str)
                    if len(off_nums) >= 2:
                        ox, oy = off_nums[0], off_nums[1]
                        paste_x = (sw - fw) // 2 + ox
                        paste_y = (sh - fh) // 2 - oy
                final_img.paste(cropped, (paste_x, paste_y), cropped)

    os.makedirs(os.path.dirname(out_png_path), exist_ok=True)
    final_img.save(out_png_path, "PNG")
    print(f"[ATLAS] Successfully extracted '{chosen_frame_key}' to {out_png_path} ({final_img.size[0]}x{final_img.size[1]})")
    return True

def extract_sleigh_sprite_from_atlas(atlas_png_path, plist_path, out_png_path):
    if not (os.path.isfile(atlas_png_path) and os.path.isfile(plist_path)):
        return False
    try:
        with open(plist_path, "rb") as f:
            pl = plistlib.load(f)
    except Exception as e:
        print(f"Warning: Failed to parse plist {plist_path}: {e}")
        return False

    frames = pl.get("frames", {})
    if not frames:
        return False

    all_keys = list(frames.keys())
    print(f"[ATLAS] Sleigh candidate frames in {plist_path}: {all_keys}")

    # Prioritize body/chassis/sleigh without runner/ski/reindeer/circle
    def sleigh_score(k):
        kl = k.lower()
        if "reindeer" in kl or "circle" in kl or "runner" in kl or "ski" in kl or "leg" in kl:
            return -100
        score = 0
        if "sleigh" in kl:
            score += 50
        if "small" in kl or "body" in kl or "chassis" in kl or "hull" in kl:
            score += 30
        return score

    sorted_candidates = sorted(all_keys, key=sleigh_score, reverse=True)
    chosen_frame_key = sorted_candidates[0] if sorted_candidates and sleigh_score(sorted_candidates[0]) > -50 else None
    if not chosen_frame_key:
        chosen_frame_key = next(iter(frames.keys()))

    print(f"[ATLAS] Selected frame for sleigh: '{chosen_frame_key}'")

    finfo = frames[chosen_frame_key]
    frame_str = finfo.get("frame") or finfo.get("textureRect")
    rotated = finfo.get("rotated", False) or finfo.get("textureRotated", False)
    source_size_str = finfo.get("sourceSize") or finfo.get("spriteSourceSize")
    source_color_rect_str = finfo.get("sourceColorRect") or finfo.get("spriteColorRect")
    offset_str = finfo.get("offset") or finfo.get("spriteOffset")

    if not frame_str:
        return False

    f_nums = parse_plist_rect(frame_str)
    if len(f_nums) < 4:
        return False
    fx, fy, fw, fh = f_nums[0], f_nums[1], f_nums[2], f_nums[3]

    try:
        atlas_img = Image.open(atlas_png_path).convert("RGBA")
    except Exception as e:
        print(f"Warning: Failed to open atlas image {atlas_png_path}: {e}")
        return False

    if rotated:
        crop_box = (fx, fy, fx + fh, fy + fw)
    else:
        crop_box = (fx, fy, fx + fw, fy + fh)

    cropped = atlas_img.crop(crop_box)
    if rotated:
        cropped = cropped.transpose(Image.ROTATE_270)

    final_img = cropped
    if source_size_str:
        ss_nums = parse_plist_rect(source_size_str)
        if len(ss_nums) >= 2:
            sw, sh = ss_nums[0], ss_nums[1]
            if sw > 0 and sh > 0:
                final_img = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
                paste_x, paste_y = 0, 0
                if source_color_rect_str:
                    cr_nums = parse_plist_rect(source_color_rect_str)
                    if len(cr_nums) >= 4:
                        paste_x, paste_y = cr_nums[0], cr_nums[1]
                elif offset_str:
                    off_nums = parse_plist_rect(offset_str)
                    if len(off_nums) >= 2:
                        ox, oy = off_nums[0], off_nums[1]
                        paste_x = (sw - fw) // 2 + ox
                        paste_y = (sh - fh) // 2 - oy
                final_img.paste(cropped, (paste_x, paste_y), cropped)

    os.makedirs(os.path.dirname(out_png_path), exist_ok=True)
    final_img.save(out_png_path, "PNG")
    print(f"[ATLAS] Successfully extracted '{chosen_frame_key}' to {out_png_path} ({final_img.size[0]}x{final_img.size[1]})")
    return True

def convert_audio_to_wav(data_or_path, dst_wav_path, target_rate=22050):
    """Converts input audio (bytes or file) to 16-bit PCM mono WAV at target_rate."""
    os.makedirs(os.path.dirname(dst_wav_path), exist_ok=True)
    temp_input = None
    if isinstance(data_or_path, (bytes, bytearray)):
        with tempfile.NamedTemporaryFile(suffix=".ogg", delete=False) as tf:
            tf.write(data_or_path)
            temp_input = tf.name
        src_path = temp_input
    else:
        src_path = data_or_path

    success = False
    temp_wav = dst_wav_path + ".tmp.wav"
    try:
        # 1. Try ffmpeg if available
        cmd = ["ffmpeg", "-y", "-i", src_path, "-ar", str(target_rate), "-ac", "1", "-c:a", "pcm_s16le", dst_wav_path]
        res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if res.returncode == 0 and os.path.exists(dst_wav_path) and os.path.getsize(dst_wav_path) > 44:
            success = True
    except Exception:
        pass

    if not success:
        try:
            # 2. Try oggdec if available
            cmd = ["oggdec", "-o", temp_wav, src_path]
            res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if res.returncode == 0 and os.path.exists(temp_wav):
                with wave.open(temp_wav, "rb") as r:
                    nchannels = r.getnchannels()
                    sampwidth = r.getsampwidth()
                    framerate = r.getframerate()
                    nframes = r.getnframes()
                    raw = r.readframes(nframes)
                
                # Convert to mono 16-bit
                samples = []
                fmt = "<h" if sampwidth == 2 else "<b"
                step = sampwidth * nchannels
                for i in range(0, len(raw), step):
                    ch_sum = 0
                    for ch in range(nchannels):
                        val = struct.unpack_from(fmt, raw, i + ch * sampwidth)[0]
                        ch_sum += val
                    samples.append(int(ch_sum / nchannels))
                
                # Resample to target_rate if needed
                if framerate != target_rate and len(samples) > 1:
                    ratio = float(target_rate) / float(framerate)
                    new_len = int(len(samples) * ratio)
                    resampled = []
                    for ni in range(new_len):
                        src_idx = ni / ratio
                        i0 = int(src_idx)
                        i1 = min(i0 + 1, len(samples) - 1)
                        frac = src_idx - i0
                        s = int(samples[i0] * (1.0 - frac) + samples[i1] * frac)
                        resampled.append(s)
                    samples = resampled
                
                with wave.open(dst_wav_path, "wb") as w:
                    w.setnchannels(1)
                    w.setsampwidth(2)
                    w.setframerate(target_rate)
                    out_bytes = struct.pack(f"<{len(samples)}h", *samples)
                    w.writeframes(out_bytes)
                success = True
        except Exception:
            pass
        finally:
            if os.path.exists(temp_wav):
                try: os.remove(temp_wav)
                except Exception: pass

    if temp_input and os.path.exists(temp_input):
        try: os.remove(temp_input)
        except Exception: pass

    if not success:
        print(f"  [WARNING] Could not decode/convert {os.path.basename(src_path)} to 22050Hz WAV (no ffmpeg/oggdec).")
    return success

def next_pot(x):
    p = 1
    while p < x:
        p <<= 1
    return p

def swizzle_image(data_bytes, width, height, bytes_per_pixel=4):
    """Swizzles an image for PSP hardware GE (16 bytes by 8 lines blocks)"""
    out = bytearray(len(data_bytes))
    block_width = 16 // bytes_per_pixel  # in pixels (4 for 8888, 8 for 5551)
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

def convert_image_to_tex(img_data_or_path, dst_path, target_format=FORMAT_RGBA8888, swizzle=True):
    """Converts a PNG image (file path or bytes) to PSP .tex format"""
    if isinstance(img_data_or_path, (bytes, bytearray)):
        img = Image.open(io.BytesIO(img_data_or_path))
    else:
        img = Image.open(img_data_or_path)

    img = img.convert('RGBA')
    orig_w, orig_h = img.size

    # Handle textures > 512: Downscale with aspect ratio preservation (NOT silent cropping)
    if orig_w > 512 or orig_h > 512:
        scale = 512.0 / max(orig_w, orig_h)
        new_w = max(1, int(round(orig_w * scale)))
        new_h = max(1, int(round(orig_h * scale)))
        resample_filter = getattr(Image, 'Resampling', Image).LANCZOS if hasattr(Image, 'Resampling') else Image.ANTIALIAS
        img = img.resize((new_w, new_h), resample_filter)
        print(f"  [RESIZE] {os.path.basename(dst_path)} downscaled from {orig_w}x{orig_h} to {new_w}x{new_h} to fit PSP 512 max")
        orig_w, orig_h = new_w, new_h

    # Ensure POT dimensions; min 16 width and 8 height for swizzle blocks
    pot_w = max(16, min(512, next_pot(orig_w)))
    pot_h = max(8, min(512, next_pot(orig_h)))

    # Create POT canvas
    pot_img = Image.new('RGBA', (pot_w, pot_h), (0, 0, 0, 0))
    if orig_w < pot_w and (pot_w % orig_w) == 0 and orig_h == pot_h:
        for x_off in range(0, pot_w, orig_w):
            pot_img.paste(img, (x_off, 0))
        orig_w = pot_w
    else:
        pot_img.paste(img, (0, 0))
    raw_pixels = pot_img.tobytes()

    if target_format == FORMAT_RGBA5551:
        bpp = 2
        # Convert 32bpp RGBA to 16bpp A1R5G5B5 (little-endian for PSP)
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
    else: # FORMAT_RGBA8888
        bpp = 4
        pixel_bytes = raw_pixels

    final_bytes = swizzle_image(pixel_bytes, pot_w, pot_h, bpp) if swizzle else pixel_bytes

    # Header: magic(4), orig_w(2), orig_h(2), pot_w(2), pot_h(2), fmt(2), swizzle(2) = 16 bytes
    header = struct.pack('<4sHHHHHH', PSP_TEX_MAGIC, orig_w, orig_h, pot_w, pot_h, target_format, 1 if swizzle else 0)

    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    with open(dst_path, 'wb') as f:
        f.write(header)
        f.write(final_bytes)

    return orig_w, orig_h, pot_w, pot_h, target_format, len(header) + len(final_bytes)

def get_target_format(filename, cli_format):
    if cli_format == "5551":
        return FORMAT_RGBA5551
    elif cli_format == "8888":
        return FORMAT_RGBA8888

    # auto mode:
    fname = os.path.basename(filename).lower()

    # Garage and stage preview icons, coins, and canisters use RGBA5551 to preserve PSP RAM
    if fname.startswith("car-icon-") or fname.startswith("stage-icon-") or "coin" in fname or "gas" in fname:
        return FORMAT_RGBA5551

    # High-detail vehicle body and wheel components in RGBA8888
    vehicle_keywords = [
        "car.png", "tire.png", "car-", "bike-", "-tire", "wheel", "tank-", "tractor", "driver-"
    ]
    if any(k in fname for k in vehicle_keywords):
        return FORMAT_RGBA8888

    # High-detail props
    if fname in ["antenna.png", "bridgewoodblock.png"]:
        return FORMAT_RGBA8888

    # Default: 5551 for terrain ground/surface, pedals, buttons, fonts
    return FORMAT_RGBA5551

def process_assets(apk_path=None, assets_dir=None, out_dir="assets", format_mode="auto", swizzle=True, convert_audio=True, convert_music=True):
    print("==================================================")
    print("   Hill Climb Racing: Asset Preprocessing Pipeline")
    print("==================================================")
    print(f"Output directory: {out_dir}")
    print(f"Format mode:      {format_mode}")
    print(f"Swizzling:        {'Enabled (PSP GE)' if swizzle else 'Disabled (Linear)'}")
    print(f"Audio conversion: {'Enabled (16-bit PCM 22050Hz)' if convert_audio else 'Disabled'}")

    os.makedirs(out_dir, exist_ok=True)
    manifest = []
    processed_count = 0
    audio_count = 0

    if apk_path:
        print(f"Source APK:       {apk_path}")
        if not os.path.isfile(apk_path):
            raise FileNotFoundError(f"APK file not found: {apk_path}")

        with zipfile.ZipFile(apk_path, 'r') as z:
            namelist = z.namelist()
            # Filter assets
            asset_entries = [f for f in namelist if f.startswith("assets/") and not f.endswith("/")]
            if not asset_entries:
                # If no assets/ prefix, try top-level files
                asset_entries = [f for f in namelist if not f.endswith("/")]

            for entry in asset_entries:
                rel_path = entry[len("assets/"):] if entry.startswith("assets/") else entry
                dst_path = os.path.join(out_dir, rel_path)
                data = z.read(entry)

                if entry.lower().endswith(".png"):
                    tex_path = os.path.splitext(dst_path)[0] + ".tex"
                    png_path = os.path.splitext(dst_path)[0] + ".png"
                    # Also write .png for PC compatibility
                    os.makedirs(os.path.dirname(png_path), exist_ok=True)
                    with open(png_path, "wb") as pf:
                        pf.write(data)
                    fmt = get_target_format(entry, format_mode)
                    ow, oh, pw, ph, fcode, fsize = convert_image_to_tex(data, tex_path, target_format=fmt, swizzle=swizzle)
                    manifest.append({
                        "file": rel_path,
                        "tex": os.path.splitext(rel_path)[0] + ".tex",
                        "orig": f"{ow}x{oh}",
                        "pot": f"{pw}x{ph}",
                        "format": "RGBA8888" if fcode == FORMAT_RGBA8888 else "RGBA5551",
                        "size": fsize
                    })
                    processed_count += 1
                elif entry.lower().endswith((".ogg", ".wav", ".mp3")):
                    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                    with open(dst_path, "wb") as f:
                        f.write(data)
                    if convert_audio:
                        is_music = any(m in entry.lower() for m in ["bgmusic", "music", "jinglebells"])
                        if is_music and not convert_music:
                            pass
                        else:
                            wav_path = os.path.splitext(dst_path)[0] + ".wav"
                            if convert_audio_to_wav(data, wav_path):
                                audio_count += 1
                    processed_count += 1
                else:
                    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                    with open(dst_path, "wb") as f:
                        f.write(data)
                    processed_count += 1

    elif assets_dir:
        print(f"Source Directory: {assets_dir}")
        if not os.path.isdir(assets_dir):
            raise FileNotFoundError(f"Assets directory not found: {assets_dir}")

        for root, _, files in os.walk(assets_dir):
            for f in sorted(files):
                src_path = os.path.join(root, f)
                rel_path = os.path.relpath(src_path, assets_dir)
                dst_path = os.path.join(out_dir, rel_path)

                if f.lower().endswith(".png"):
                    tex_path = os.path.splitext(dst_path)[0] + ".tex"
                    png_path = os.path.splitext(dst_path)[0] + ".png"
                    # Preserve/copy .png for PC
                    if src_path != png_path:
                        os.makedirs(os.path.dirname(png_path), exist_ok=True)
                        shutil.copy2(src_path, png_path)
                    fmt = get_target_format(f, format_mode)
                    ow, oh, pw, ph, fcode, fsize = convert_image_to_tex(src_path, tex_path, target_format=fmt, swizzle=swizzle)
                    manifest.append({
                        "file": rel_path,
                        "tex": os.path.splitext(rel_path)[0] + ".tex",
                        "orig": f"{ow}x{oh}",
                        "pot": f"{pw}x{ph}",
                        "format": "RGBA8888" if fcode == FORMAT_RGBA8888 else "RGBA5551",
                        "size": fsize
                    })
                    processed_count += 1
                elif f.lower().endswith((".ogg", ".wav", ".mp3")):
                    if src_path != dst_path:
                        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                        shutil.copy2(src_path, dst_path)
                    if convert_audio:
                        is_music = any(m in f.lower() for m in ["bgmusic", "music", "jinglebells"])
                        if is_music and not convert_music:
                            pass
                        else:
                            wav_path = os.path.splitext(dst_path)[0] + ".wav"
                            if convert_audio_to_wav(src_path, wav_path):
                                audio_count += 1
                    processed_count += 1
                else:
                    if src_path != dst_path:
                        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                        shutil.copy2(src_path, dst_path)
                    processed_count += 1

    # T-131b: Extract sleigh chassis sprite from atlas/sleigh.plist if available
    sleigh_png = os.path.join(out_dir, "car-sleigh.png")
    sleigh_tex = os.path.join(out_dir, "car-sleigh.tex")
    atlas_png = os.path.join(out_dir, "atlas", "sleigh.png")
    atlas_plist = os.path.join(out_dir, "atlas", "sleigh.plist")
    if extract_sleigh_sprite_from_atlas(atlas_png, atlas_plist, sleigh_png):
        fmt = FORMAT_RGBA8888
        ow, oh, pw, ph, fcode, fsize = convert_image_to_tex(sleigh_png, sleigh_tex, target_format=fmt, swizzle=swizzle)
        manifest.append({
            "file": "car-sleigh.png",
            "tex": "car-sleigh.tex",
            "orig": f"{ow}x{oh}",
            "pot": f"{pw}x{ph}",
            "format": "RGBA8888",
            "size": fsize
        })
        processed_count += 1

    # Extract hippievan chassis and tire from atlas/car-hippievan.plist if available
    hv_atlas_png = os.path.join(out_dir, "atlas", "car-hippievan.png")
    hv_atlas_plist = os.path.join(out_dir, "atlas", "car-hippievan.plist")
    for frame_target in ["car-hippievan-chassis.png", "car-hippievan-tire.png"]:
        dst_png = os.path.join(out_dir, frame_target)
        dst_tex = os.path.splitext(dst_png)[0] + ".tex"
        if extract_frame_from_atlas(hv_atlas_png, hv_atlas_plist, frame_target, dst_png):
            fmt = FORMAT_RGBA8888
            ow, oh, pw, ph, fcode, fsize = convert_image_to_tex(dst_png, dst_tex, target_format=fmt, swizzle=swizzle)
            manifest.append({
                "file": frame_target,
                "tex": os.path.basename(dst_tex),
                "orig": f"{ow}x{oh}",
                "pot": f"{pw}x{ph}",
                "format": "RGBA8888",
                "size": fsize
            })
            processed_count += 1

    print(f"\nProcessing complete! Successfully processed {processed_count} files into '{out_dir}'.")
    print(f"Converted {len(manifest)} textures to PSP-native swizzled .tex format.")
    if convert_audio:
        print(f"Converted {audio_count} audio files to canonical 22050Hz 16-bit PCM WAV.")
    return manifest

def audit_assets(out_dir="assets", allow_missing=False, stages_cfg="config/stages.cfg", vehicles_cfg="config/vehicles.cfg"):
    import configparser
    print("\n==================================================")
    print("   Hill Climb Racing: Canonical Asset Audit (H17a)")
    print("==================================================")

    if not (os.path.isfile(stages_cfg) and os.path.isfile(vehicles_cfg)):
        print(f"Warning: Configuration files not found ({stages_cfg}, {vehicles_cfg}). Skipping audit.")
        return True

    cp_stages = configparser.ConfigParser()
    cp_stages.read(stages_cfg, encoding="utf-8")
    cp_vehicles = configparser.ConfigParser()
    cp_vehicles.read(vehicles_cfg, encoding="utf-8")

    stages = cp_stages.sections()
    vehicles = cp_vehicles.sections()

    missing_files = []
    
    stage_png_ok = 0
    stage_tex_ok = 0
    stage_icon_png_ok = 0
    stage_icon_tex_ok = 0
    music_ok = 0

    STAGE_ICON_ALIAS = {
        "mountain": "cliffs",
        "mudpool": "mud",
        "arcticcave": "arctic-cave",
    }

    print(f"[AUDIT] Auditing {len(stages)} Stages...")
    for s in stages:
        icon_id = STAGE_ICON_ALIAS.get(s, s)
        icon_png = os.path.join(out_dir, f"stage-icon-{icon_id}.png")
        icon_tex = os.path.join(out_dir, f"stage-icon-{icon_id}.tex")
        if os.path.isfile(icon_png):
            stage_icon_png_ok += 1
        else:
            missing_files.append(f"Stage [{s}] icon PNG missing: {icon_png}")
        if os.path.isfile(icon_tex):
            stage_icon_tex_ok += 1
        else:
            missing_files.append(f"Stage [{s}] icon TEX missing: {icon_tex}")

        for key in ["ground_texture", "surface_texture"]:
            tex_val = cp_stages.get(s, key, fallback="")
            if not tex_val:
                missing_files.append(f"Stage [{s}] missing key '{key}' in config")
                continue
            rel = tex_val[len("assets/"):] if tex_val.startswith("assets/") else tex_val
            png_path = os.path.join(out_dir, rel)
            tex_path = os.path.splitext(png_path)[0] + ".tex"
            
            if os.path.isfile(png_path):
                stage_png_ok += 1
            else:
                missing_files.append(f"Stage [{s}] PNG missing: {png_path}")

            if os.path.isfile(tex_path):
                stage_tex_ok += 1
            else:
                missing_files.append(f"Stage [{s}] TEX missing: {tex_path}")

        music_val = cp_stages.get(s, "music", fallback="")
        if music_val:
            rel_m = music_val[len("assets/"):] if music_val.startswith("assets/") else music_val
            m_path = os.path.join(out_dir, rel_m)
            m_wav = os.path.splitext(m_path)[0] + ".wav"
            m_ogg = os.path.splitext(m_path)[0] + ".ogg"
            if os.path.isfile(m_path) or os.path.isfile(m_wav) or os.path.isfile(m_ogg):
                music_ok += 1
            else:
                missing_files.append(f"Stage [{s}] Music missing: {m_path} (.wav/.ogg)")
        else:
            missing_files.append(f"Stage [{s}] missing 'music' in config")

    veh_png_ok = 0
    veh_tex_ok = 0
    print(f"[AUDIT] Auditing {len(vehicles)} Vehicles...")
    for v in vehicles:
        for key in ["body_texture", "wheel_texture", "preview_texture"]:
            tex_val = cp_vehicles.get(v, key, fallback="")
            if not tex_val:
                missing_files.append(f"Vehicle [{v}] missing key '{key}' in config")
                continue
            rel = tex_val[len("assets/"):] if tex_val.startswith("assets/") else tex_val
            png_path = os.path.join(out_dir, rel)
            tex_path = os.path.splitext(png_path)[0] + ".tex"

            if os.path.isfile(png_path):
                veh_png_ok += 1
            else:
                missing_files.append(f"Vehicle [{v}] PNG missing: {png_path}")

            if os.path.isfile(tex_path):
                veh_tex_ok += 1
            else:
                missing_files.append(f"Vehicle [{v}] TEX missing: {tex_path}")

    print("\n--------------------------------------------------")
    print(f"Stage Textures:   PNG {stage_png_ok}/{len(stages)*2} | TEX {stage_tex_ok}/{len(stages)*2}")
    print(f"Stage Icons:      PNG {stage_icon_png_ok}/{len(stages)} | TEX {stage_icon_tex_ok}/{len(stages)}")
    print(f"Vehicle Textures: PNG {veh_png_ok}/{len(vehicles)*3} | TEX {veh_tex_ok}/{len(vehicles)*3}")
    print(f"Stage Music:      AUDIO {music_ok}/{len(stages)}")
    print("--------------------------------------------------")

    if missing_files:
        print(f"[AUDIT FAILED] Found {len(missing_files)} missing asset entries:")
        for m in missing_files[:20]:
            print(f"  - {m}")
        if len(missing_files) > 20:
            print(f"  ... and {len(missing_files) - 20} more.")
        if not allow_missing:
            print("[AUDIT] Exiting with status 1 (use --allow-missing to ignore).")
            sys.exit(1)
        else:
            print("[AUDIT] Continuing because --allow-missing was passed.")
            return False
    else:
        print(f"[AUDIT PASSED] 100% Asset Parity Verified: {len(stages)} Stages, {len(vehicles)} Vehicles, {len(stages)} Music tracks.")
        return True

def main():
    parser = argparse.ArgumentParser(description="Hill Climb Racing - Asset Converter & Preprocessor (T-111 & T-043)")
    input_group = parser.add_mutually_exclusive_group(required=False)
    input_group.add_argument("--apk", type=str, help="Path to original Hill Climb Racing APK (e.g. hcr_1.0.5.apk)")
    input_group.add_argument("--assets", type=str, help="Path to raw/source assets directory")
    parser.add_argument("--out", type=str, default="assets", help="Output directory for canonical assets (default: assets)")
    parser.add_argument("--format", choices=["5551", "8888", "auto"], default="auto", help="Texture format (default: auto)")
    parser.add_argument("--no-swizzle", action="store_true", help="Disable PSP GE hardware texture swizzling")
    parser.add_argument("--no-audio", action="store_true", help="Disable audio conversion to 22050Hz WAV")
    parser.add_argument("--no-music", action="store_true", help="Disable background music tracks conversion")
    parser.add_argument("--allow-missing", action="store_true", help="Do not exit with error code if asset audit finds missing files")

    args = parser.parse_args()

    apk_path = args.apk
    assets_dir = args.assets

    # If neither argument provided, check default workspace locations
    if not apk_path and not assets_dir:
        default_apk = os.path.join("original_apk", "hcr_1.0.5.apk")
        default_assets = os.path.join("original_apk", "assets")
        if os.path.isfile(default_apk):
            apk_path = default_apk
        elif os.path.isdir(default_assets):
            assets_dir = default_assets
        elif os.path.isdir("assets"):
            assets_dir = "assets"
        else:
            parser.error("No input provided and default APK/assets not found. Use --apk PATH or --assets DIR.")

    process_assets(
        apk_path=apk_path,
        assets_dir=assets_dir,
        out_dir=args.out,
        format_mode=args.format,
        swizzle=not args.no_swizzle,
        convert_audio=not args.no_audio,
        convert_music=not args.no_music
    )

    audit_assets(
        out_dir=args.out,
        allow_missing=args.allow_missing
    )

if __name__ == '__main__':
    main()
