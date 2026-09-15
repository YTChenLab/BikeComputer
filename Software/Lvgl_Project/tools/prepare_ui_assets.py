#!/usr/bin/env python3
"""Prepare PC and STM32 background assets for the 480 x 854 LVGL display."""

from math import ceil
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parent.parent
SOURCE = PROJECT_ROOT / "assets" / "background" / "bg_6_master.jpg"
RAW_OUTPUT = PROJECT_ROOT / "assets" / "generated" / "bg_6_480x854.rgb565"
JPEG_OUTPUT = PROJECT_ROOT / "assets" / "generated" / "bg_6_q75.jpg"
TARGET_SIZE = (480, 854)


def resize_cover(image: Image.Image, target_size: tuple[int, int]) -> Image.Image:
    """Scale to cover the display and crop equally from opposing edges."""
    target_width, target_height = target_size
    source_width, source_height = image.size
    scale = max(target_width / source_width, target_height / source_height)
    resized_size = (ceil(source_width * scale), ceil(source_height * scale))

    # Near-matching assets need no meaningful crop. A direct high-quality resize
    # avoids preferentially removing a one-pixel edge feature.
    if abs(resized_size[0] - target_width) <= 2 and abs(resized_size[1] - target_height) <= 2:
        return image.resize(target_size, Image.Resampling.LANCZOS)

    resized = image.resize(resized_size, Image.Resampling.LANCZOS)

    crop_left = (resized.width - target_width) // 2
    crop_top = (resized.height - target_height) // 2
    return resized.crop((crop_left, crop_top, crop_left + target_width, crop_top + target_height))


def encode_rgb565_little_endian(image: Image.Image) -> bytes:
    """Encode LV_COLOR_FORMAT_RGB565 for this little-endian UCRT64 simulator."""
    raw = bytearray(image.width * image.height * 2)
    write_index = 0

    for red, green, blue in image.get_flattened_data():
        rgb565 = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
        raw[write_index] = rgb565 & 0xFF
        raw[write_index + 1] = rgb565 >> 8
        write_index += 2

    return bytes(raw)


def main() -> None:
    if not SOURCE.is_file():
        raise FileNotFoundError(f"Background master image not found: {SOURCE}")

    RAW_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(SOURCE) as source_image:
        source_size = source_image.size
        source_mode = source_image.mode
        prepared = resize_cover(source_image.convert("RGB"), TARGET_SIZE)

    raw_data = encode_rgb565_little_endian(prepared)
    RAW_OUTPUT.write_bytes(raw_data)

    # RGB conversion creates a fresh image, so source EXIF/ICC metadata is not carried over.
    prepared.save(
        JPEG_OUTPUT,
        format="JPEG",
        quality=75,
        subsampling=2,
        progressive=False,
        optimize=True,
    )

    raw_bytes = RAW_OUTPUT.stat().st_size
    jpeg_bytes = JPEG_OUTPUT.stat().st_size
    print(f"source size : {source_size}")
    print(f"source mode : {source_mode}")
    print(f"output size : {prepared.size}")
    print(f"RAW size    : {raw_bytes} bytes (RGB565 little-endian)")
    print(f"JPEG size   : {jpeg_bytes} bytes (quality=75, 4:2:0 baseline)")
    print(f"JPEG ratio  : {raw_bytes / jpeg_bytes:.1f}:1 versus RGB565 RAW")


if __name__ == "__main__":
    main()
