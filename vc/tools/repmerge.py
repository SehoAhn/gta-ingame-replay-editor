"""Merge a SUB replay into a MAIN replay -> one .rep with both players (P0 prototype).

usage: repmerge.py MAIN SUB DST --offset-ms N [--sub-pedtype T] [--force]
  N = when the sub replay starts, in ms relative to the main start (negative = earlier).
  The project range is min(main start, sub start) .. max(main end, sub end).

Playback rules (gta-vc.exe v1.0, PlayBackThisFrameInterpolation 0x621230):
  * per frame the game walks vehicles (VEHICLE 1 / BIKE 2 share one cursor) and peds
    (PED_UPDATE 4) in ascending pool index; every entity whose index is skipped, and
    everything after the last one at ENDOFFRAME, is DELETED -> packets must be sorted
    by index, and an entity missing from a frame disappears (= "hidden outside its range");
  * PED_HEADER [3][idx][mi u16][pedtype] creates the ped if its slot is empty
    (pedtype 0 -> CPlayerPed, else CCivilianPed); a PED_UPDATE without a ped logs
    "Replay Ped wasn't there" -> a header is emitted whenever a ped is not in the previous
    output frame or not yet in this buffer;
  * PED_UPDATE +3 = vehicle index + 1 (0 = on foot); VEHICLE colours +0x2D/+0x2E,
    BIKE +0x28/+0x29; GENERAL = camera + player position; CLOCK / WEATHER / TIMER / MISC
    are global -> taken from MAIN only (first / last main frame outside the main range).
  * BULLET_TRACES (10) use a global trace slot (byte 3) -> sub ones dropped.
Frame order kept as recorded: PARTICLE*, GENERAL, CLOCK, WEATHER, TIMER, vehicles,
(PED_HEADER) PED_UPDATE..., BULLET*, MISC, ENDOFFRAME.
More than 8 buffers is fine for F6 PLAY of the Replay Editor (window player).
"""
import argparse
import bisect
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from repparse import BUFSZ, HEADER, load, frames  # noqa: E402

PED_POOL, VEH_POOL = 140, 110


def read(path):
    out = []
    for fr in frames(load(path)):
        pk = [(t, bytes(raw)) for t, _b, _o, raw in fr]
        tm = [struct.unpack_from("<I", r, 4)[0] for t, r in pk if t == 9]
        if not tm:
            continue
        out.append((tm[0], pk))
    return out


def used(fr_list, types):
    s = set()
    for _t, pk in fr_list:
        for t, r in pk:
            if t in types:
                s.add(r[1])
    return s


def remap_table(main_used, sub_used, pool, what):
    free = [i for i in range(pool) if i not in main_used]
    if len(free) < len(sub_used):
        sys.exit("not enough free %s slots: main uses %d, sub needs %d of %d" % (what, len(main_used), len(sub_used), pool))
    # keep sub indices that are free anyway, give the others the lowest free ones
    m, taken = {}, set()
    for i in sorted(sub_used):
        if i not in main_used:
            m[i] = i; taken.add(i)
    rest = [i for i in free if i not in taken]
    for i in sorted(sub_used):
        if i not in m:
            m[i] = rest.pop(0)
    return m


def sub_packet(t, r, pmap, vmap, pedtype):
    b = bytearray(r)
    if t in (1, 2):
        b[1] = vmap[b[1]]
    elif t == 3:
        b[1] = pmap[b[1]]
        if pedtype is not None and b[4] == 0:
            b[4] = pedtype
    elif t == 4:
        b[1] = pmap[b[1]]
        if b[3]:
            b[3] = vmap[b[3] - 1] + 1
    return bytes(b)


def build(main, sub, offset_ms, pedtype):
    m0, s0 = main[0][0], sub[0][0]
    mt = [t - m0 for t, _ in main]                      # main times rel. to main start
    st = [t - s0 + offset_ms for t, _ in sub]           # sub times on the main time axis
    m_end, s_beg, s_end = mt[-1], st[0], st[-1]
    pmap = remap_table(used(main, (3, 4)), used(sub, (3, 4)), PED_POOL, "ped")
    vmap = remap_table(used(main, (1, 2)), used(sub, (1, 2)), VEH_POOL, "vehicle")

    # output frame times: sub frames before the main start, main frames, sub frames after the main end
    times = [t for t in st if t < 0] + mt + [t for t in st if t > m_end]
    first_glob = [(t, r) for t, r in main[0][1] if t in (5, 6, 7, 12)]
    last_glob = [(t, r) for t, r in main[-1][1] if t in (5, 6, 7, 12)]
    base = m0 if m0 + times[0] > 0 else -times[0] + 1   # TIMER stays positive

    out, prev_si = [], None
    for T in times:
        mi = bisect.bisect_right(mt, T) - 1 if 0 <= T <= m_end else None
        si = bisect.bisect_right(st, T) - 1 if s_beg <= T <= s_end else None
        mpk = main[mi][1] if mi is not None else (first_glob if T < 0 else last_glob)
        spk = [(t, sub_packet(t, r, pmap, vmap, pedtype)) for t, r in sub[si][1]] if si is not None else []
        g = lambda pk, ts: [(t, r) for t, r in pk if t in ts]
        frame = g(mpk, (11,)) + (g(spk, (11,)) if si != prev_si else [])   # repeated sub frame: no double effects
        prev_si = si
        frame += g(mpk, (5, 6, 7))
        frame += [(9, struct.pack("<II", 9, base + T))]
        frame += sorted(g(mpk, (1, 2)) + g(spk, (1, 2)), key=lambda p: p[1][1])
        frame += [("hdr", r) for t, r in mpk + spk if t == 3]
        frame += sorted(g(mpk, (4,)) + g(spk, (4,)), key=lambda p: p[1][1])
        frame += g(mpk, (10,)) + g(mpk, (12,))
        frame.append((8, struct.pack("<I", 8)))
        out.append(frame)
    info = dict(frames=len(out), main=len(main), sub=len(sub), dur_ms=times[-1] - times[0],
                start_ms=times[0], pmap=pmap, vmap=vmap,
                pre=sum(1 for t in times if t < 0), post=sum(1 for t in times if t > m_end))
    return out, info


def encode(out_frames):
    """frames -> 100000-byte buffers; PED_HEADER emitted per buffer and on (re)appearance."""
    header = {}
    bufs, cur, emitted, prev_peds = [], bytearray(), set(), set()

    def enc(fr, emitted_now):
        data, em, peds = bytearray(), set(emitted_now), set()
        for t, r in fr:
            if t == "hdr":
                header[r[1]] = r
                continue
            if t == 4:
                i = r[1]
                if (i not in em or i not in prev_peds) and i in header:
                    data += header[i]
                    em.add(i)
                peds.add(i)
            data += r
        return data, em, peds

    for fr in out_frames:
        for t, r in fr:                     # headers of this frame are known before its updates
            if t == "hdr":
                header[r[1]] = r
        data, em, peds = enc(fr, emitted)
        if len(cur) + len(data) + 1 > BUFSZ:
            bufs.append(bytes(cur))
            cur, emitted = bytearray(), set()
            data, em, peds = enc(fr, emitted)
            if len(data) + 1 > BUFSZ:
                raise ValueError("single frame larger than a buffer")
        cur += data
        emitted, prev_peds = em, peds
    bufs.append(bytes(cur))
    return [b + bytes(BUFSZ - len(b)) for b in bufs]


def validate(bufs, nframes):
    n = 0
    for bi, b in enumerate(bufs):
        prev, seen = set(), set()
        for fr in frames([b]):
            n += 1
            vlast, plast, cur = -1, -1, set()
            for t, _bi, _off, raw in fr:
                if t in (1, 2):
                    assert raw[1] > vlast, "buf %d: vehicles not ascending" % bi
                    vlast = raw[1]
                elif t == 3:
                    seen.add(raw[1])
                elif t == 4:
                    assert raw[1] > plast, "buf %d: peds not ascending" % bi
                    assert raw[1] in seen, "buf %d: PED_UPDATE %d before its header" % (bi, raw[1])
                    plast = raw[1]
                    cur.add(raw[1])
            prev = cur
    assert n == nframes, "frame count %d != %d" % (n, nframes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("main"); ap.add_argument("sub"); ap.add_argument("dst")
    ap.add_argument("--offset-ms", type=int, default=0)
    ap.add_argument("--sub-pedtype", type=int, default=None,
                    help="pedtype for the sub player (default: keep 0 = CPlayerPed)")
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args()
    main_fr, sub_fr = read(a.main), read(a.sub)
    out, info = build(main_fr, sub_fr, a.offset_ms, a.sub_pedtype)
    bufs = encode(out)
    validate(bufs, len(out))
    if os.path.exists(a.dst) and not a.force:
        sys.exit("refusing to overwrite %s (use --force)" % a.dst)
    with open(a.dst, "wb") as fh:
        fh.write(HEADER)
        for b in bufs:
            fh.write(b)
    print("%s + %s (offset %+d ms) -> %s" % (os.path.basename(a.main), os.path.basename(a.sub), a.offset_ms, a.dst))
    print("  frames %d (main %d, sub %d; %d before main, %d after main), %.2f s, %d buffers" % (
        info["frames"], info["main"], info["sub"], info["pre"], info["post"], info["dur_ms"] / 1000.0, len(bufs)))
    print("  sub ped map %s, vehicle map %s" % (info["pmap"], info["vmap"]))


if __name__ == "__main__":
    main()
