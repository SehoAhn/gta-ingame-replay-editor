"""GTA VC v1.0 replay.rep parser / validator.

File layout (from SaveReplayToHD 0x61F200 / PlayReplayFromHD 0x61F0E0):
  8-byte header "gtaVC7f\\0", then N x 100000-byte buffers, oldest first,
  last one is the "current" slot. Inside a buffer: packet stream, 0 = end.
"""
import struct
import sys
from collections import Counter

BUFSZ = 100000
HEADER = b"gtaVC7f\x00"
SIZES = {1: 0x34, 2: 0x2C, 3: 0x08, 4: 0x2C, 5: 0x58, 6: 0x04, 7: 0x08,
         8: 0x04, 9: 0x08, 10: 0x1C, 11: 0x14, 12: 0x10}
NAMES = {0: "END", 1: "VEHICLE", 2: "BIKE", 3: "PED_HEADER", 4: "PED_UPDATE",
         5: "GENERAL", 6: "CLOCK", 7: "WEATHER", 8: "ENDOFFRAME", 9: "TIMER",
         10: "BULLET_TRACES", 11: "PARTICLE", 12: "MISC"}


def load(path):
    data = open(path, "rb").read()
    if data[:8] != HEADER:
        raise ValueError("bad header %r" % data[:8])
    body = data[8:]
    # The game reads 100000-byte chunks while Read() returns non-zero, so a
    # short last chunk (written by external tools) is accepted as a buffer.
    return [body[i:i + BUFSZ].ljust(BUFSZ, bytes(1)) for i in range(0, len(body), BUFSZ)]


def frames(bufs):
    """Yield frames: dict(buf, packets=[(type, offset, bytes)])."""
    cur = []
    for bi, b in enumerate(bufs):
        off = 0
        while True:
            if off >= BUFSZ:
                break
            t = b[off]
            if t == 0:
                break
            if t not in SIZES:
                raise ValueError("buf %d off %#x: unknown packet %d" % (bi, off, t))
            sz = SIZES[t]
            cur.append((t, bi, off, b[off:off + sz]))
            off += sz
            if t == 8:
                yield cur
                cur = []
        # a frame may start with PARTICLE packets at the tail of one buffer and
        # continue in the next (particles are written between frames)
    if cur:
        yield cur


def main(path):
    bufs = load(path)
    cnt = Counter()
    nfr = 0
    times = []
    first_types = []
    peds_hdr_per_buf = Counter()
    for fr in frames(bufs):
        nfr += 1
        types = [p[0] for p in fr]
        if len(first_types) < 1:
            first_types = types
        for t, bi, off, raw in fr:
            cnt[t] += 1
            if t == 9:
                times.append(struct.unpack_from("<I", raw, 4)[0])
            if t == 3:
                peds_hdr_per_buf[bi] += 1
        assert types.count(5) == 1, "frame has %d GENERAL packets" % types.count(5)
    print(path)
    print("  buffers=%d frames=%d" % (len(bufs), nfr))
    if times:
        dur = (times[-1] - times[0]) / 1000.0
        print("  time %d..%d ms  (%.2f s, %.1f fps avg)" % (times[0], times[-1], dur,
                                                             (nfr - 1) / dur if dur else 0))
    print("  packets:", ", ".join("%s=%d" % (NAMES[k], v) for k, v in sorted(cnt.items())))
    print("  first frame order:", [NAMES[t] for t in first_types][:8], "...")
    print("  PED_HEADER per buffer:", dict(peds_hdr_per_buf))


if __name__ == "__main__":
    for p in sys.argv[1:]:
        try:
            main(p)
        except Exception as e:  # report and continue with other files
            print(p, "ERROR:", e)
