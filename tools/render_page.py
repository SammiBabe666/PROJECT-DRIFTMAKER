#!/usr/bin/env python3
"""Draw a DRIFTMAKER page from the drawing calls its own page code made (host/dm_host page ...) into a PNG.

    host/dm_host page TAG GROUP FRAMES [TOUCH [FREEZE]] | python3 tools/render_page.py OUT.png [--scale 3]

A PREVIEW on the computer, not the OP-1's renderer: the lines, curves, dots, bars and text are drawn where and how the
page asked, in the stock encoder colours, with a plain sans in capitals standing in for the OP-1's font. Only the
part the OP-1's window shows (y 2-146) is bright; the rest is dimmed. Needs Pillow."""
import sys
from PIL import Image, ImageDraw, ImageFont

COL = {-10: (40, 120, 255), -11: (0, 200, 110), -12: (215, 215, 240), -13: (255, 80, 40),
       14: (255, 255, 255), 12: (125, 125, 125), 2: (230, 40, 40), 1: (0, 0, 0), 4: (60, 60, 60)}
FONTS = ['/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf']


def colour(c):
    c = int(c)
    if c >= 0 and c & 0x01000000:
        return ((c >> 16) & 255, (c >> 8) & 255, c & 255)
    return COL.get(c, (255, 0, 255))


def bezier(p0, p1, p2, p3, n=16):
    out = []
    for i in range(1, n + 1):
        t = i / n
        a, b, c, d = (1 - t) ** 3, 3 * t * (1 - t) ** 2, 3 * t * t * (1 - t), t ** 3
        out.append((a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]))
    return out


def render(lines, scale=3):
    W, H = 320 * scale, 160 * scale
    img = Image.new('RGB', (W, H), (0, 0, 0))
    d = ImageDraw.Draw(img)
    px = scale / 1.175                        # a panel pixel (rev 2: 376 x 188 for 320 x 160 units)
    font_path = next((f for f in FONTS if __import__('os').path.exists(f)), None)
    subs, cur = [], None

    def U(v):
        return float(v) / 256 * scale

    for ln in lines:
        w = ln.rstrip('\n').split(' ')
        op = w[0]
        if op == 'path':
            subs, cur = [], None
        elif op == 'move':
            cur = [(U(w[1]), U(w[2]))]
            subs.append(cur)
        elif op == 'lineto':
            if cur is None:
                cur = []; subs.append(cur)
            cur.append((U(w[1]), U(w[2])))
        elif op == 'curveto':
            if cur is None or not cur:
                cur = [(U(w[5]), U(w[6]))]; subs.append(cur)
            else:
                cur.extend(bezier(cur[-1], (U(w[1]), U(w[2])), (U(w[3]), U(w[4])), (U(w[5]), U(w[6]))))
        elif op == 'close':
            if cur:
                cur.append(cur[0])
        elif op == 'stroke':
            width = max(1, round(int(w[1]) / 256 * px))
            for s in subs:
                if len(s) > 1:
                    d.line(s, fill=colour(w[2]), width=width, joint='curve')
        elif op == 'fillpath':
            for s in subs:
                if len(s) > 2:
                    d.polygon(s, fill=colour(w[1]))
        elif op == 'sline':
            width = max(1, round(int(w[5]) / 256 * px))
            d.line([(U(w[1]), U(w[2])), (U(w[3]), U(w[4]))], fill=colour(w[6]), width=width)
        elif op == 'line':
            d.line([(U(w[1]), U(w[2])), (U(w[3]), U(w[4]))], fill=colour(w[5]), width=max(1, round(px)))
        elif op == 'dot':
            x, y, r = int(w[1]) * scale, int(w[2]) * scale, int(w[3]) * scale
            if int(w[4]):
                d.ellipse([x - r, y - r, x + r, y + r], fill=colour(w[5]))
            else:
                d.ellipse([x - r, y - r, x + r, y + r], outline=colour(w[5]), width=max(1, round(px)))
        elif op in ('fill', 'frame'):
            x0, y0, x1, y1 = (int(v) * scale for v in w[1:5])
            if op == 'fill':
                d.rectangle([x0, y0, x1, y1], fill=colour(w[5]))
            else:
                d.rectangle([x0, y0, x1, y1], outline=colour(w[5]), width=max(1, round(px)))
        elif op == 'bar':
            x0, y0, x1, y1 = (int(v) * scale for v in w[1:5])
            val, mx = int(w[5]), max(1, int(w[6]))
            d.rectangle([x0, y0, x1, y1], fill=COL[4])
            xm = x0 + (x1 - x0) * min(val, mx) // mx
            if xm > x0:
                d.rectangle([x0, y0, xm, y1], fill=colour(w[7]))
        elif op == 'text':
            x, y, size, align, c = (int(v) for v in w[1:6])
            text = ' '.join(w[6:]).upper()
            f = ImageFont.truetype(font_path, max(6, int(size * scale * 0.95))) if font_path else ImageFont.load_default()
            tw = d.textlength(text, font=f)
            xx = x * scale - (tw if align == 1 else tw / 2 if align == 2 else 0)
            d.text((xx, y * scale - size * scale * 0.18), text, fill=colour(c), font=f)
    # the OP-1's window shows y 2-146: dim what it hides
    shade = Image.new('RGB', (W, H), (0, 0, 0))
    mask = Image.new('L', (W, H), 0)
    ImageDraw.Draw(mask).rectangle([0, 147 * scale, W, H], fill=150)
    ImageDraw.Draw(mask).rectangle([0, 0, W, 2 * scale], fill=150)
    img.paste(shade, (0, 0), mask)
    return img


if __name__ == '__main__':
    out = sys.argv[1]
    scale = int(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else 3
    render(sys.stdin.readlines(), scale).save(out)
