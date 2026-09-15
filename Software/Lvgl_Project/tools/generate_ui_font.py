#!/usr/bin/env python3
"""Generate the compact MiSans CJK LVGL font used by pages P0-P7."""

from pathlib import Path
import re

from PIL import Image, ImageDraw, ImageFont


PROJECT_ROOT = Path(__file__).resolve().parent.parent
FONT_PATH = PROJECT_ROOT / "assets" / "fonts_src" / "MiSans-Semibold.ttf"
OUTPUT_PATH = PROJECT_ROOT / "src" / "ui" / "fonts" / "ui_font_misans_ui_20.c"
METRIC_FONT_PATH = PROJECT_ROOT / "assets" / "fonts_src" / "MiSans-Heavy.ttf"
METRIC_OUTPUT_PATH = PROJECT_ROOT / "src" / "ui" / "fonts" / "ui_font_misans_metric_60.c"
NUMBER_68_OUTPUT_PATH = PROJECT_ROOT / "src" / "ui" / "fonts" / "ui_font_misans_num_68.c"
UI_SOURCE_DIR = PROJECT_ROOT / "src" / "ui"
FONT_NAME = "ui_font_misans_ui_20"
FONT_SIZE = 20
METRIC_FONT_NAME = "ui_font_misans_metric_60"
METRIC_FONT_SIZE = 60
NUMBER_68_FONT_NAME = "ui_font_misans_num_68"
NUMBER_68_FONT_SIZE = 68
METRIC_SYMBOLS = "0123456789.-"
ASCII_SYMBOLS = "".join(chr(codepoint) for codepoint in range(32, 127))
UI_PUNCTUATION = "·℃—…，。！？：；（）【】《》“”‘’％"

# Keep this list limited to runtime UI strings; English/numbers use existing MiSans fallback fonts.
SYMBOLS = (
    "智能骑行码表系统启动中初始化显示屏卫星定位蓝牙连接存储卡"
    "导航剩余距离预计到达累计爬升地图设置路线轨迹显示放大小返回"
    "完成总平均速度最大热量心率功率保存记录上传数据主页"
    "个人中心用户等级次数贡献最近查看详情成就首次百公里挑战连续已解锁"
    "传感器声音电池模式网络关于设备亮度自动息屏时间主题音量按键提示标准省电低功耗语言"
    "固件版本恢复出厂信息型号序列号空间健康"
    "快捷控制开始暂停结束选择确认取消已连接未开启关闭搜星良好"
    "下作为全部前务单尊当或操新有服机没源滑演的程继缩者菜请进重"
    "\u5148\u5165\u5c0a\u5c1a"
)


def collect_runtime_ui_symbols() -> str:
    """Keep the font in sync with actual C string literals, not only a manual list."""
    literals = []
    for source_path in UI_SOURCE_DIR.rglob("*.c"):
        if "fonts" in source_path.parts:
            continue
        source = source_path.read_text(encoding="utf-8")
        literals.extend(re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', source))
    return "".join(
        character
        for literal in literals
        for character in literal
        if "\u4e00" <= character <= "\u9fff"
    )


def pack_4bpp(mask: Image.Image) -> bytes:
    # Pillow 12 deprecates getdata() for direct image pixels.
    values = list(mask.get_flattened_data())
    packed = bytearray()
    for index in range(0, len(values), 2):
        high = values[index] >> 4
        low = values[index + 1] >> 4 if index + 1 < len(values) else 0
        packed.append((high << 4) | low)
    return bytes(packed)


def c_bytes(data: bytes) -> str:
    lines = []
    for start in range(0, len(data), 12):
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in data[start:start + 12]) + ",")
    return "\n".join(lines)


def generate_font(font_path: Path, output_path: Path, font_name: str, font_size: int,
                  symbols: str, fallback_name: str | None = None,
                  metric_ascii_cmaps: bool = False) -> None:
    font = ImageFont.truetype(font_path, font_size)
    glyphs = []
    min_top = 0
    max_bottom = 0

    for character in sorted(set(symbols)):
        left, top, right, bottom = font.getbbox(character, anchor="ls")
        canvas = Image.new("L", (right - left, bottom - top), 0)
        ImageDraw.Draw(canvas).text((-left, -top), character, font=font, fill=255, anchor="ls")
        nonempty = canvas.getbbox()
        if nonempty is None:
            continue

        crop = canvas.crop(nonempty)
        glyph_left = left + nonempty[0]
        glyph_top = top + nonempty[1]
        glyph_bottom = top + nonempty[3]
        min_top = min(min_top, glyph_top)
        max_bottom = max(max_bottom, glyph_bottom)
        glyphs.append((character, pack_4bpp(crop), round(font.getlength(character) * 16),
                       crop.width, crop.height, glyph_left, -glyph_bottom))

    line_height = max_bottom - min_top
    base_line = max_bottom
    bitmap = bytearray()
    glyph_entries = []
    for character, data, advance, width, height, offset_x, offset_y in glyphs:
        glyph_entries.append((len(bitmap), advance, width, height, offset_x, offset_y))
        bitmap.extend(data)

    codepoints = [ord(character) for character, *_ in glyphs]
    glyph_dsc = "\n".join(
        f"    {{.bitmap_index = {index}, .adv_w = {advance}, .box_w = {width}, .box_h = {height}, .ofs_x = {offset_x}, .ofs_y = {offset_y}}},"
        for index, advance, width, height, offset_x, offset_y in glyph_entries
    )
    if metric_ascii_cmaps:
        # Keep punctuation and digits in two contiguous cmap ranges.  This is
        # both smaller and avoids sparse-cmap edge cases for U+002D in LVGL.
        cmap_tables = """static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 45, .range_length = 2, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 48, .range_length = 10, .glyph_id_start = 3,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};"""
        cmap_count = 2
    else:
        range_start = codepoints[0]
        offsets = [codepoint - range_start for codepoint in codepoints]
        unicode_list = ", ".join(f"0x{offset:x}" for offset in offsets)
        cmap_tables = f"""static const uint16_t unicode_list_0[] = {{ {unicode_list} }};

static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{
        .range_start = {range_start}, .range_length = {codepoints[-1] - range_start + 1}, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = {len(codepoints)}, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }}
}};"""
        cmap_count = 1

    fallback_declaration = f"extern const lv_font_t {fallback_name};\n\n" if fallback_name else ""
    fallback_value = f"&{fallback_name}" if fallback_name else "NULL"
    output = f'''/*******************************************************************************
 * Generated by tools/generate_ui_font.py from {font_path.name}.
 * Size: {font_size}px, Bpp: 4, glyphs: {len(glyphs)}
 ******************************************************************************/
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

{fallback_declaration}static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {{
{c_bytes(bytes(bitmap))}
}};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
    {{.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0}},
{glyph_dsc}
}};

{cmap_tables}

static const lv_font_fmt_txt_dsc_t font_dsc = {{
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = {cmap_count},
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
}};

const lv_font_t {font_name} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {line_height},
    .base_line = {base_line},
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -2,
    .underline_thickness = 1,
    .dsc = &font_dsc,
    .fallback = {fallback_value},
    .user_data = NULL,
}};
'''
    output_path.write_text(output, encoding="utf-8", newline="\n")
    print(f"generated: {output_path.relative_to(PROJECT_ROOT)}")
    print(f"glyphs: {len(glyphs)}, bitmap: {len(bitmap)} bytes, line height: {line_height}")


def main() -> None:
    # Generate all printable ASCII here as well, so mixed strings such as
    # "50.25 km", firmware versions and percentages can never lose glyphs.
    runtime_symbols = SYMBOLS + collect_runtime_ui_symbols() + ASCII_SYMBOLS + UI_PUNCTUATION
    generate_font(FONT_PATH, OUTPUT_PATH, FONT_NAME, FONT_SIZE, runtime_symbols,
                  fallback_name="ui_font_misans_en_18")
    generate_font(METRIC_FONT_PATH, METRIC_OUTPUT_PATH, METRIC_FONT_NAME,
                  METRIC_FONT_SIZE, METRIC_SYMBOLS, metric_ascii_cmaps=True)
    generate_font(METRIC_FONT_PATH, NUMBER_68_OUTPUT_PATH, NUMBER_68_FONT_NAME,
                  NUMBER_68_FONT_SIZE, METRIC_SYMBOLS, metric_ascii_cmaps=True)


if __name__ == "__main__":
    main()
