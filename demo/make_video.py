#!/usr/bin/env python3
"""Assemble DRIFTMAKER_DEMO.mp4 from the engine's own audio and the page's own drawing calls.

    python3 demo/make_score.py > demo/score.txt
    host/dm_host video demo/score.txt build/demo.raw > build/frames.txt
    python3 demo/make_video.py demo/score.txt build/frames.txt build/demo.raw DRIFTMAKER_DEMO.mp4 [--jobs 4]

The audio is DRIFTMAKER's engine code (the same C that the .opmod is built from) run on a computer through the host
model of the OP-1 (host/emul.c). The screen is tools/render_page.py's preview of the drawing calls the page code made at
each 30th of a second. Nothing here is footage of an OP-1, and every frame says so. Needs Pillow and ffmpeg."""
import functools
import os
import subprocess
import sys
from multiprocessing import Pool

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
from PIL import Image, ImageDraw, ImageFont  # noqa: E402

ImageFont.truetype = functools.lru_cache(maxsize=64)(ImageFont.truetype)   # render_page asks per text call
import render_page  # noqa: E402

W, H, FPS, RATE = 1920, 1080, 30, 44100
BG = (9, 9, 11)
ENC = [(40, 120, 255), (0, 200, 110), (215, 215, 240), (255, 80, 40)]
ENC_NAME = ['core', 'motion', 'texture', 'space']
SX, SY, SCALE = 80, 150, 4                      # the screen preview: 1280 x 640
RX = 1420                                       # the right column
D = '/usr/share/fonts/truetype/dejavu/'
J = '/usr/share/fonts/truetype/jetbrains-mono/'
FOOT = ("audio: DRIFTMAKER's engine code rendered on a computer through a model of the OP-1's voices. "
        "screen: a preview drawn from the page code's own drawing calls.",
        "not footage of an OP-1. tested in the host model and the Blackfin simulator only.")


def font(name, size):
    path = (J if name.startswith('JetBrains') else D) + name
    return ImageFont.truetype(path, size)


# ---------------------------------------------------------------- the score: what is shown when
def read_score(path):
    now, ev = 0.0, []
    for ln in open(path):
        ln = ln.rstrip('\n')
        if not ln or ln.startswith('#'):
            continue
        op, _, rest = ln.partition(' ')
        if op == 't':
            now = float(rest)
        else:
            ev.append((now, op, rest))
    return ev, now


class State:
    """The overlays at time t, from the score's events up to t."""

    def __init__(self, events, end):
        self.ev, self.end = events, end

    def at(self, t):
        s = dict(title=None, section=None, sec_t=0, caption=None, cap_t=0, card=None, card_t=0, end=None, end_t=0,
                 group=0, notes=set(), turned=[-9.0] * 4, pushed=-9.0)
        for (et, op, rest) in self.ev:
            if et > t:
                break
            if op == 'title':
                s['title'] = rest
            elif op == 'section':
                s.update(section=rest, sec_t=et, caption=None, card=None)
            elif op == 'caption':
                s.update(caption=rest, cap_t=et)
            elif op == 'preset_card':
                s.update(card=rest, card_t=et)
            elif op == 'end_card':
                s.update(end=rest, end_t=et)
            elif op == 'key':
                g = int(rest) - 59
                if 0 <= g < 4:
                    s['group'], s['pushed'] = g, et
            elif op == 'turn':
                s['turned'][int(rest.split()[0])] = et
            elif op == 'on':
                s['notes'].add(int(rest))
            elif op == 'off':
                s['notes'].discard(int(rest))
            elif op == 'alloff':
                s['notes'] = set()
        return s


def fade(t, t0, dur=0.6):
    return max(0.0, min(1.0, (t - t0) / dur))


def dim(c, a):
    return tuple(int(BG[i] + (c[i] - BG[i]) * a) for i in range(3))


def spaced(d, xy, text, f, fill, track=4, anchor_right=False):
    x, y = xy
    if anchor_right:
        x -= sum(d.textlength(ch, font=f) + track for ch in text) - track
    for ch in text:
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + track


def wrap(d, text, f, width):
    out = []
    for para in text.split('|'):
        line = ''
        for w in para.split(' '):
            trial = (line + ' ' + w).strip()
            if d.textlength(trial, font=f) <= width or not line:
                line = trial
            else:
                out.append(line)
                line = w
        out.append(line)
    return out


# ---------------------------------------------------------------- one frame
def compose(t, page_lines, s, total):
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)

    # the screen
    scr = render_page.render(page_lines, SCALE)
    img.paste(scr, (SX, SY))
    d.rounded_rectangle([SX - 14, SY - 14, SX + 1280 + 14, SY + 640 + 14], radius=18, outline=(52, 52, 58), width=2)
    spaced(d, (SX, SY + 668), 'page preview  ·  t1 screen, 320 x 160', font('JetBrainsMono-Light.ttf', 18), (90, 90, 98), 2)

    # the wordmark
    spaced(d, (SX, 62), 'DRIFTMAKER', font('JetBrainsMono-Medium.ttf', 34), (235, 235, 240), 10)
    spaced(d, (SX + 1280, 72), 'opus 2 beta 4  ·  op-1', font('JetBrainsMono-Light.ttf', 20), (120, 120, 128), 3, True)

    # the section and caption, or the preset card
    if s['section']:
        a = fade(t, s['sec_t'])
        spaced(d, (RX, SY + 4), s['section'].upper(), font('JetBrainsMono-Regular.ttf', 22), dim((150, 150, 160), a), 3)
        d.line([RX, SY + 44, RX + 420, SY + 44], fill=dim((60, 60, 66), a), width=2)
    y = SY + 74
    if s['card']:
        a = fade(t, s['card_t'], 0.4)
        n, name, line = s['card'].split('|')
        f = font('JetBrainsMono-Thin.ttf', 96)
        d.text((RX, y - 10), f'{int(n):02d}', font=f, fill=dim(ENC[(int(n) - 1) % 4], a))
        d.text((RX + 150, y + 52), '/ 16', font=font('JetBrainsMono-Light.ttf', 26), fill=dim((110, 110, 118), a))
        y += 130
        for ln in wrap(d, name, font('DejaVuSans.ttf', 44), 430):
            d.text((RX, y), ln, font=font('DejaVuSans.ttf', 44), fill=dim((240, 240, 244), a)); y += 56
        y += 10
        for ln in wrap(d, line, font('DejaVuSans.ttf', 26), 430):
            d.text((RX, y), ln, font=font('DejaVuSans.ttf', 26), fill=dim((170, 170, 178), a)); y += 36
    elif s['caption']:
        a = fade(t, s['cap_t'])
        f = font('DejaVuSans.ttf', 28)
        for ln in wrap(d, s['caption'], f, 430):
            d.text((RX, y), ln, font=f, fill=dim((225, 225, 232), a)); y += 40

    # the encoders: the page's colour ring is thick, a turn lights it
    ey = SY + 560
    spaced(d, (RX, ey - 78), 'ENCODERS  ·  PAGE ' + ENC_NAME[s['group']].upper(), font('JetBrainsMono-Light.ttf', 18),
           (120, 120, 128), 2)
    for i in range(4):
        cx, r = RX + 46 + i * 112, 36
        lit = max(0.0, 1.0 - (t - s['turned'][i]) / 0.35)
        pushed = i == s['group'] and t - s['pushed'] < 0.3
        if lit > 0 or pushed:
            d.ellipse([cx - r, ey - r, cx + r, ey + r], fill=dim(ENC[i], 0.35 + 0.65 * max(lit, pushed)))
        d.ellipse([cx - r, ey - r, cx + r, ey + r], outline=ENC[i], width=6 if i == s['group'] else 2)
        d.ellipse([cx - 4, ey - r + 8, cx + 4, ey - r + 16], fill=ENC[i])

    # the keys held: three octaves from c2
    kx, ky, kw, kh = SX, SY + 720, 1280 / 22, 62
    whites = [n for n in range(36, 74) if n % 12 in (0, 2, 4, 5, 7, 9, 11)]
    for j, n in enumerate(whites):
        on = n in s['notes']
        d.rectangle([kx + j * kw + 2, ky, kx + (j + 1) * kw - 2, ky + kh], fill=(235, 235, 240) if on else (34, 34, 38))
    for j, n in enumerate(whites):
        if n % 12 in (0, 2, 5, 7, 9) and n + 1 < 74:
            on = n + 1 in s['notes']
            x = kx + (j + 1) * kw
            d.rectangle([x - kw * 0.3, ky, x + kw * 0.3, ky + kh * 0.58], fill=(255, 80, 40) if on else (14, 14, 16))

    # the honest footer
    f = font('DejaVuSans.ttf', 19)
    d.text((SX, H - 92), FOOT[0], font=f, fill=(112, 112, 120))
    d.text((SX, H - 64), FOOT[1], font=f, fill=(112, 112, 120))

    # the title and end cards over everything
    if s['title'] and t < 7.0:
        draw_card(img, s['title'], 1.0 - fade(t, 5.6, 1.4), t)
    if s['end']:
        draw_card(img, s['end'], fade(t, s['end_t'], 1.0), t, end=True)
    blk = min(fade(t, 0.0, 1.2), 1.0 - fade(t, total - 1.5, 1.5))
    if blk < 1.0:
        img = Image.blend(Image.new('RGB', (W, H), (0, 0, 0)), img, max(0.0, blk))
    return img


def draw_card(img, text, a, t, end=False):
    if a <= 0:
        return
    over = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(over)
    parts = text.split('|')
    head = parts[0].upper()
    f = font('JetBrainsMono-Thin.ttf', 132 if not end else 110)
    tw = sum(d.textlength(ch, font=f) + 18 for ch in head) - 18
    spaced(d, ((W - tw) / 2, 330), head, f, (240, 240, 244), 18)
    d.line([W / 2 - 260, 545, W / 2 + 260, 545], fill=(70, 70, 78), width=2)
    for i in range(4):
        d.ellipse([W / 2 - 75 + i * 50 - 9, 510 - 9, W / 2 - 75 + i * 50 + 9, 510 + 9], fill=ENC[i])
    y = 585
    for k, p in enumerate(parts[1:]):
        f2 = font('DejaVuSans.ttf', 36 if k == 0 else 26)
        tw = d.textlength(p, font=f2)
        d.text(((W - tw) / 2, y), p, font=f2, fill=(210, 210, 216) if k == 0 else (140, 140, 148))
        y += 60 if k == 0 else 44
    f3 = font('DejaVuSans.ttf', 19)
    for k, ln in enumerate(FOOT):
        tw = d.textlength(ln, font=f3)
        d.text(((W - tw) / 2, H - 92 + 28 * k), ln, font=f3, fill=(112, 112, 120))
    img.paste(Image.blend(img, over, a))


# ---------------------------------------------------------------- the frames file
def index_frames(path):
    """Byte offsets of each "frame SAMPLES" line (one number: "frame x0 y0 x1 y1 c" is a drawing call)."""
    out = []
    with open(path, 'rb') as f:
        pos = 0
        for ln in f:
            if ln.startswith(b'frame ') and ln.count(b' ') == 1:
                out.append((pos, int(ln.split()[1])))
            pos += len(ln)
        out.append((pos, None))
    return out


G = {}


def init(score, frames, total):
    ev, end = read_score(score)
    G.update(state=State(ev, end), frames=frames, total=total)


def job(arg):
    i, start, stop, sample = arg
    with open(G['frames'], 'rb') as f:
        f.seek(start)
        lines = f.read(stop - start).decode().splitlines()[1:]
    t = sample / RATE
    return compose(t, lines, G['state'].at(t), G['total']).tobytes()


def main():
    a = sys.argv[1:]
    jobs = int(a[a.index('--jobs') + 1]) if '--jobs' in a else os.cpu_count()
    only = a[a.index('--still') + 1] if '--still' in a else None
    score, frames, audio, out = a[:4]
    total = os.path.getsize(audio) / 2 / RATE
    idx = index_frames(frames)
    work = [(i, idx[i][0], idx[i + 1][0], idx[i][1]) for i in range(len(idx) - 1)]
    if only:                                    # --still SECONDS: one frame to OUT as a PNG, for checking
        init(score, frames, total)
        i = min(range(len(work)), key=lambda k: abs(work[k][3] / RATE - float(only)))
        Image.frombytes('RGB', (W, H), job(work[i])).save(out)
        return
    ff = subprocess.Popen(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{W}x{H}',
                           '-r', str(FPS), '-i', '-', '-f', 's16le', '-ar', str(RATE), '-ac', '1', '-i', audio,
                           '-c:v', 'libx264', '-preset', 'medium', '-crf', '20', '-pix_fmt', 'yuv420p',
                           '-c:a', 'aac', '-b:a', '192k', '-ar', '48000', '-shortest', '-movflags', '+faststart', out],
                          stdin=subprocess.PIPE)
    with Pool(jobs, init, (score, frames, total)) as pool:
        for n, b in enumerate(pool.imap(job, work, chunksize=8)):
            ff.stdin.write(b)
            if n % 900 == 0:
                print(f'{n}/{len(work)} frames', file=sys.stderr, flush=True)
    ff.stdin.close()
    sys.exit(ff.wait())


if __name__ == '__main__':
    main()
