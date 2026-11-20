#!/usr/bin/env python3
"""
A move as recorded, played once (sprint 105), cut from a take (cycles.Take reads it). Out, at 60 frames a second:
each joint against the hips as cycles.py has them, in legs; when each foot's down; and its marks.

A jump: around the longest stretch it's off the ground, facing as it faced taking off. Marked where it leaves the
ground and comes back to it, so a player can fit the flight to however long it's in the air.

A strike (--strike, the end that strikes: a wrist or an ankle): around one of that end's strikes, where it's
furthest out ahead of the body, the one nearest --at seconds or else the furthest. Facing as it faced striking;
marked where it lands (hit), what a game would hit with.

A flying strike (--flying, the end that strikes): cut as a jump, and marked where that end's furthest out ahead
in the air.

  clip.py subjects/105/105_39.amc jump > jump.json   (from a run, a jump's a stride of it: Strides)
  clip.py subjects/143/143_23.amc punch_right --strike RightWrist --at 2.1 > punch_right.json
  clip.py subjects/75/75_16.amc flying_kick --flying RightAnkle > flying_kick.json
"""
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # (cycles beside it, run isolated or not)
import cycles

BEFORE, AFTER, FPS = 0.3, 0.6, 60.0  # s before it leaves the ground, s after it's back; frames a second out
WIND_UP, RECOVERS = 0.35, 0.45      # s of a strike before it lands, and after

def ahead_of(t, joint):
    """Each frame, how far a joint is out ahead of the hips, the way it faces (legs)."""
    out = []
    for i, p in enumerate(t.P):
        y = t.yaw[i]
        d = cycles.sub(p[t.idx[joint]], p[t.idx['Hips']])
        out.append((d[0] * math.sin(y) + d[2] * math.cos(y)) / t.leg)
    return out

def cut(t, name, start, stop, facing, marks):
    frames, down = [], []
    f = float(start)
    while f <= stop:
        i0 = min(int(f), t.n - 2)
        u = f - i0
        p = [[t.P[i0][j][c] + (t.P[i0 + 1][j][c] - t.P[i0][j][c]) * u for c in range(3)] for j in range(len(t.P[0]))]
        frames.append(t.frame(p, facing))
        down.append([t.feet[side][round(f)] for side in (0, 1)])
        f += t.fps / FPS
    to = lambda k: round((k - start) * FPS / t.fps)
    out = {'name': name, 'joints': cycles.NAMES, 'fps': FPS, 'frames': frames, 'down': down}
    out.update({k: to(v) for k, v in marks.items()})
    return out

def strike(t, name, joint, at):
    reach = ahead_of(t, joint)
    near = round(t.fps * 0.3)
    peaks = [i for i in range(near, t.n - near) if reach[i] > 0.4 and reach[i] == max(reach[i - near:i + near + 1])]
    if not peaks:
        sys.exit('no strike found')
    peak = min(peaks, key=lambda i: abs(i / t.fps - at)) if at is not None else max(peaks, key=lambda i: reach[i])
    start = max(0, peak - round(WIND_UP * t.fps))
    stop = min(t.n - 1, peak + round(RECOVERS * t.fps))
    c = cut(t, name, start, stop, t.yaw[peak], {'hit': peak, 'off': start, 'back': stop})
    c['pace'] = 0
    c['strikes'] = joint
    json.dump(c, sys.stdout)
    print(f"{name}: {len(c['frames'])} frames, {joint} lands at {c['hit']} ({peak / t.fps:.2f} s in the take, {reach[peak]:.2f} legs out); "
          f"the take's strikes at " + ' '.join(f"{i / t.fps:.2f}" for i in peaks), file=sys.stderr)

def main():
    args = sys.argv[1:]
    opt = lambda k: (args.pop(args.index(k) + 1), args.remove(k))[0] if k in args else None
    joint, at, flying = opt('--strike'), opt('--at'), opt('--flying')
    path, name = args[0], args[1]
    t = cycles.Take(path)
    if joint:
        return strike(t, name, joint, float(at) if at is not None else None)
    # its flight: the longest stretch with neither foot down
    best, i = (0, 0), 0
    while i < t.n:
        if not t.feet[0][i] and not t.feet[1][i]:
            k = i
            while k < t.n and not t.feet[0][k] and not t.feet[1][k]:
                k += 1
            if k - i > best[1] - best[0]:
                best = (i, k)
            i = k
        else:
            i += 1
    off, back = best
    if back - off < t.fps * 0.15:
        sys.exit('no flight found')
    start = max(0, off - round(BEFORE * t.fps))
    stop = min(t.n - 1, back + round(AFTER * t.fps))
    marks = {'off': off, 'back': back}
    if flying:
        reach = ahead_of(t, flying)
        marks['hit'] = max(range(off, back), key=lambda i: reach[i])
    c = cut(t, name, start, stop, t.yaw[off], marks)
    went = cycles.sub(t.P[back][0], t.P[off][0])
    c['pace'] = math.hypot(went[0], went[2]) / ((back - off) / t.fps) / t.leg
    json.dump(c, sys.stdout)
    shown = ''.join('B' if d[0] and d[1] else 'R' if d[0] else 'L' if d[1] else '-' for d in c['down'][::3])
    print(f"{name}: {len(c['frames'])} frames, off at {c['off']}, back at {c['back']} ({(back - off) / t.fps:.2f} s up), "
          f"{shown}", file=sys.stderr)

if __name__ == '__main__':
    main()
