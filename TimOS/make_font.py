"""
Generate a custom 8px-wide monospaced Adafruit GFX font header from Consolas.
Follows the exact Adafruit GFX fontconvert bitmap packing spec:
  - Each glyph's bitmap starts at a fresh byte boundary
  - Bits are packed MSB-first within each byte
  - The last byte of each glyph is zero-padded
"""
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "C:/Windows/Fonts/consola.ttf"
FONT_SIZE = 11          # Tweak this to control glyph height (~10-11px tall)
X_ADVANCE = 8           # Forced monospaced advance width
FIRST_CHAR = 0x20       # Space
LAST_CHAR = 0x7E        # Tilde
LINE_HEIGHT = 13        # yAdvance (line spacing)

def render_glyph(font, char):
    """Render a single character and return (pixels_2d, width, height, xOffset, yOffset)."""
    # Use a generous canvas so no clipping occurs
    canvas_size = 32
    img = Image.new('L', (canvas_size, canvas_size), 0)
    draw = ImageDraw.Draw(img)
    
    # Draw at a known origin so we can measure the ink bounds
    origin_x, origin_y = 4, 20  # baseline position
    draw.text((origin_x, origin_y), char, font=font, fill=255, anchor="ls")
    
    # Find the bounding box of actual ink pixels
    pixels = img.load()
    min_x, min_y, max_x, max_y = canvas_size, canvas_size, -1, -1
    for y in range(canvas_size):
        for x in range(canvas_size):
            if pixels[x, y] > 100:  # threshold for 1-bit
                if x < min_x: min_x = x
                if x > max_x: max_x = x
                if y < min_y: min_y = y
                if y > max_y: max_y = y
    
    if max_x < 0:
        # Empty glyph (space)
        return [], 0, 0, 0, 0
    
    w = max_x - min_x + 1
    h = max_y - min_y + 1
    xOffset = min_x - origin_x
    yOffset = min_y - origin_y  # negative = above baseline
    
    # Extract 1-bit pixel grid
    rows = []
    for y in range(min_y, max_y + 1):
        row = []
        for x in range(min_x, max_x + 1):
            row.append(1 if pixels[x, y] > 100 else 0)
        rows.append(row)
    
    return rows, w, h, xOffset, yOffset


def pack_glyph_bitmap(rows, w, h):
    """Pack a glyph's pixel rows into bytes, MSB-first, byte-aligned per glyph."""
    glyph_bytes = []
    byte_val = 0
    bit_pos = 0
    
    for y in range(h):
        for x in range(w):
            if rows[y][x]:
                byte_val |= (0x80 >> bit_pos)
            bit_pos += 1
            if bit_pos == 8:
                glyph_bytes.append(byte_val)
                byte_val = 0
                bit_pos = 0
    
    # Flush last partial byte
    if bit_pos > 0:
        glyph_bytes.append(byte_val)
    
    return glyph_bytes


def generate():
    font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
    
    all_bitmap_bytes = []
    glyphs = []
    
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        char = chr(code)
        rows, w, h, xOff, yOff = render_glyph(font, char)
        
        offset = len(all_bitmap_bytes)
        
        if w == 0 or h == 0:
            # Space or empty
            glyphs.append((offset, 0, 0, X_ADVANCE, 0, 0))
        else:
            glyph_bytes = pack_glyph_bitmap(rows, w, h)
            all_bitmap_bytes.extend(glyph_bytes)
            glyphs.append((offset, w, h, X_ADVANCE, xOff, yOff))
    
    # Build the C header
    lines = []
    lines.append('#ifndef _CUSTOM_READER_FONT_H_')
    lines.append('#define _CUSTOM_READER_FONT_H_')
    lines.append('')
    lines.append('#include <Adafruit_GFX.h>')
    lines.append('')
    lines.append('const uint8_t CustomReaderFontBitmaps[] PROGMEM = {')
    
    for i in range(0, len(all_bitmap_bytes), 12):
        chunk = all_bitmap_bytes[i:i+12]
        lines.append('  ' + ', '.join(f'0x{b:02X}' for b in chunk) + ',')
    
    lines.append('};')
    lines.append('')
    lines.append('const GFXglyph CustomReaderFontGlyphs[] PROGMEM = {')
    
    for i, (off, w, h, xAdv, xOff, yOff) in enumerate(glyphs):
        c = chr(FIRST_CHAR + i)
        if c == '\\': c = '\\\\'
        elif c == "'": c = "\\'"
        lines.append(f"  {{ {off:5d}, {w:2d}, {h:2d}, {xAdv:2d}, {xOff:3d}, {yOff:3d} }}, // '{c}'")
    
    lines.append('};')
    lines.append('')
    lines.append('const GFXfont CustomReaderFont PROGMEM = {')
    lines.append('  (uint8_t  *)CustomReaderFontBitmaps,')
    lines.append('  (GFXglyph *)CustomReaderFontGlyphs,')
    lines.append(f'  0x{FIRST_CHAR:02X}, 0x{LAST_CHAR:02X}, {LINE_HEIGHT}')
    lines.append('};')
    lines.append('')
    lines.append('#endif // _CUSTOM_READER_FONT_H_')
    
    with open('CustomReaderFont.h', 'w') as f:
        f.write('\n'.join(lines))
    
    print(f"Generated CustomReaderFont.h")
    print(f"  Bitmap size: {len(all_bitmap_bytes)} bytes")
    print(f"  Glyphs: {len(glyphs)} ({FIRST_CHAR:#x} to {LAST_CHAR:#x})")
    print(f"  xAdvance: {X_ADVANCE}px (fits {128 // X_ADVANCE} chars on 128px screen)")


if __name__ == '__main__':
    generate()
