"""Trim a GTA VC replay.rep to frames [first, last] and repack it.

Rules (see ReplayEditor_Plan.html 5.4):
  * a frame = all packets up to and including ENDOFFRAME (PARTICLE packets
    recorded before GENERAL belong to the frame they precede);
  * a source frame may begin with PARTICLE packets at a buffer's tail; the
    output never splits a frame; each buffer ends with END(0);
  * PED_HEADER is required once per buffer before a ped's first PED_UPDATE
    (the recorder resets its "recorded" flag on every buffer switch), so
    headers are re-emitted from the last one seen in the source;
  * TIMER values are kept as recorded; at most 8 buffers.

usage: reptrim.py SRC DST (--frames A B | --seconds S E) [--force]
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from repparse import BUFSZ, HEADER, load, frames  # noqa: E402

MAX_BUFS = 8


def rebuild(src_frames, first, last):
    """src_frames: list of frames, each a list of (type, raw bytes)."""
    last_header = {}
    for fr in src_frames[:first]:
        for t, raw in fr:
            if t == 3:
                last_header[raw[1]] = raw
    bufs, cur, emitted = [], bytearray(), set()

    def encode(fr, emitted_now):
        out, emitted_local = bytearray(), set(emitted_now)
        for t, raw in fr:
            if t == 3:
                last_header[raw[1]] = raw
                if raw[1] in emitted_local:
                    continue
                emitted_local.add(raw[1])
            elif t == 4 and raw[1] not in emitted_local and raw[1] in last_header:
                out += last_header[raw[1]]
                emitted_local.add(raw[1])
            out += raw
        return out, emitted_local

    for fr in src_frames[first:last + 1]:
        saved = dict(last_header)
        data, new_emitted = encode(fr, emitted)
        if len(cur) + len(data) + 1 > BUFSZ:
            bufs.append(bytes(cur))
            cur, emitted = bytearray(), set()
            last_header = saved
            data, new_emitted = encode(fr, emitted)
            if len(data) + 1 > BUFSZ:
                raise ValueError("single frame larger than a buffer")
        cur += data
        emitted = new_emitted
    bufs.append(bytes(cur))
    if len(bufs) > MAX_BUFS:
        raise ValueError("%d buffers needed (max %d)" % (len(bufs), MAX_BUFS))
    return [b + bytes(BUFSZ - len(b)) for b in bufs]   # zero padding = END


def frame_times(src_frames):
    ts = []
    for fr in src_frames:
        tm = [struct.unpack_from("<I", raw, 4)[0] for t, raw in fr if t == 9]
        ts.append(tm[0] if tm else (ts[-1] if ts else 0))
    return ts


def validate(bufs, expect_frames):
    n = 0
    for bi, b in enumerate(bufs):
        seen = set()
        for fr in frames([b]):
            n += 1
            for t, _bi, _off, raw in fr:
                if t == 3:
                    seen.add(raw[1])
                elif t == 4 and raw[1] not in seen:
                    raise AssertionError("buf %d: PED_UPDATE %d before its header" % (bi, raw[1]))
    if n != expect_frames:
        raise AssertionError("frame count %d != %d" % (n, expect_frames))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--frames", nargs=2, type=int, metavar=("A", "B"))
    g.add_argument("--seconds", nargs=2, type=float, metavar=("S", "E"),
                   help="offsets from the start; negative E counts from the end")
    ap.add_argument("--force", action="store_true", help="overwrite DST")
    a = ap.parse_args()

    src = [[(t, raw) for t, _b, _o, raw in fr] for fr in frames(load(a.src))]
    ts = frame_times(src)
    if a.frames:
        first, last = a.frames
    else:
        t0 = ts[0]
        s = t0 + a.seconds[0] * 1000
        e = (ts[-1] + a.seconds[1] * 1000) if a.seconds[1] < 0 else t0 + a.seconds[1] * 1000
        first = next(i for i, t in enumerate(ts) if t >= s)
        last = max(i for i, t in enumerate(ts) if t <= e)
    if not 0 <= first <= last < len(src):
        sys.exit("bad range %d..%d (have %d frames)" % (first, last, len(src)))
    bufs = rebuild(src, first, last)
    validate(bufs, last - first + 1)
    if os.path.exists(a.dst) and not a.force:
        sys.exit("refusing to overwrite %s (use --force)" % a.dst)
    with open(a.dst, "wb") as fh:
        fh.write(HEADER)
        for b in bufs:
            fh.write(b)
    print("frames %d..%d of %d  (%.2f s -> %.2f s), %d buffers -> %s" % (
        first, last, len(src), (ts[-1] - ts[0]) / 1000.0, (ts[last] - ts[first]) / 1000.0,
        len(bufs), a.dst))


if __name__ == "__main__":
    main()
