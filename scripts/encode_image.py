#!/usr/bin/env python3
"""
Convert a PNG/WebP image (with alpha) to an 8bpp C header for TFT_eSPI pushImage(bpp8=true).

Composites the image onto a black background so transparent pixels become
TFT_BLACK, then resizes to fit the target display dimensions using nearest-neighbor
while maintaining aspect ratio. Outputs 8-bit 3-3-2 format (RRRGGGBB).

Usage:
    python3 encode_image.py <input_image> <output_header> <var_prefix> [--width W] [--height H]

Defaults: --width 280 --height 240  (280 = 320 - 40 for 20px left/right padding)
"""

import argparse
import os
import sys

from PIL import Image


def rgb888_to_rgb332(r, g, b):
    """Convert 8-bit RGB to 8-bit 3-3-2 format (RRRGGGBB)."""
    return ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6)


def convert_image(input_path, output_path, var_prefix, target_width=280, target_height=240):
    # Open image; preserve alpha
    img = Image.open(input_path)

    if img.mode == "P":
        img = img.convert("RGBA")
    elif img.mode == "RGB":
        img = img.convert("RGBA")

    # Composite onto black background so transparent pixels become black
    background = Image.new("RGBA", img.size, (0, 0, 0, 255))
    composited = Image.alpha_composite(background, img).convert("RGB")

    # Resize to fit within target dimensions, preserving aspect ratio (nearest-neighbor)
    composited.thumbnail((target_width, target_height), Image.NEAREST)
    w, h = composited.size

    # Convert to 8bpp (3-3-2 format: RRRGGGBB)
    pixels = composited.load()
    data = bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b = pixels[x, y]
            data.append(rgb888_to_rgb332(r, g, b))

    # Build C header
    array_name = f"{var_prefix}_data"
    width_name = f"{var_prefix}_width"
    height_name = f"{var_prefix}_height"

    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append(f"const uint8_t {array_name}[] = {{")

    # Format as hex, 16 values per line
    per_line = 16
    for i in range(0, len(data), per_line):
        chunk = data[i : i + per_line]
        hex_vals = ", ".join(f"0x{v:02X}" for v in chunk)
        lines.append(f"    {hex_vals},")

    lines.append("};")
    lines.append("")
    lines.append(f"const uint16_t {width_name}  = {w};")
    lines.append(f"const uint16_t {height_name} = {h};")
    lines.append("")

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w") as f:
        f.write("\n".join(lines) + "\n")

    pixel_count = len(data)
    size_kb = pixel_count / 1024.0
    print(f"Wrote {output_path}: {w}x{h} ({pixel_count} pixels, {size_kb:.1f} KB 8bpp)")


def main():
    parser = argparse.ArgumentParser(description="Convert image to 8bpp C header for TFT_eSPI")
    parser.add_argument("input", help="Input image path (PNG, WebP, etc.)")
    parser.add_argument("output", help="Output C header path")
    parser.add_argument("prefix", help="Variable name prefix (e.g. 'logo_broomfield_stem')")
    parser.add_argument("--width", type=int, default=280, help="Target max width (default: 280)")
    parser.add_argument("--height", type=int, default=240, help="Target max height (default: 240)")
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"Error: input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    convert_image(args.input, args.output, args.prefix, args.width, args.height)


if __name__ == "__main__":
    main()
