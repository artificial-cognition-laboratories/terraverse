#!/usr/bin/env python3
"""
A stride cycle from a motion capture (sprint 105): where each joint is, through one stride, in the walker's own frame.

Reads a BVH (100STYLE's: cm, Y up, its left at +x, 60 frames a second) or a CMU take (an .amc, its subject's .asf
beside it: asfamc.py, 120 a second), finds when each foot's on the ground (low
and still, heel or toe), cuts strides at each right foot coming down, keeps those going straight that take about as
long as the rest, and averages them into one, closed so its end runs on into its start. Out, per phase: each joint
against the hips, in lengths of its leg, x to its right, y up, z ahead; and whether each foot's down, in most strides.

  cycles.py Neutral_FW.bvh walk 327 7445 > walk.json   (100STYLE's Frame_Cuts: its forward walk)
  cycles.py Neutral_FR.bvh jog 357 4227 > jog.json     (100STYLE calls it a run: it's a jog)
  cycles.py subjects/09/09_01.amc,subjects/09/09_02.amc,... run > run.json   (each reads its subject's .asf beside it)
  cycles.py subjects/102/102_05.amc,subjects/127/127_07.amc,subjects/127/127_08.amc sprint > sprint.json
"""
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # (asfamc beside it, run isolated or not)
import asfamc, fbx

def rot(axis, deg):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    if axis == 'X': return [[1, 0, 0], [0, c, -s], [0, s, c]]
    if axis == 'Y': return [[c, 0, s], [0, 1, 0], [-s, 0, c]]
    return [[c, -s, 0], [s, c, 0], [0, 0, 1]]

def mul(a, b): return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def app(m, v): return [sum(m[i][k] * v[k] for k in range(3)) for i in range(3)]
def add(a, b): return [a[i] + b[i] for i in range(3)]
def sub(a, b): return [a[i] - b[i] for i in range(3)]
def norm(a):
    l = math.sqrt(sum(x * x for x in a)); return [x / l for x in a] if l > 1e-9 else a
def length(a): return math.sqrt(sum(x * x for x in a))

def read(path):
    lines = open(path).read().split('\n')
    joints, stack, i = [], [], 0
    while not lines[i].startswith('MOTION'):
        t = lines[i].split()
        if t and t[0] in ('ROOT', 'JOINT'):
            joints.append({'name': t[1], 'parent': stack[-1] if stack else -1, 'offset': None, 'channels': []})
            pending = len(joints) - 1
        elif t and t[0] == 'End':
            joints.append({'name': joints[stack[-1]]['name'] + '_end', 'parent': stack[-1], 'offset': None, 'channels': []})
            pending = len(joints) - 1
        elif t and t[0] == '{':
            stack.append(pending)
        elif t and t[0] == '}':
            stack.pop()
        elif t and t[0] == 'OFFSET':
            joints[stack[-1]]['offset'] = list(map(float, t[1:4]))
        elif t and t[0] == 'CHANNELS':
            joints[stack[-1]]['channels'] = t[2:]
        i += 1
    frames = [list(map(float, l.split())) for l in lines[i + 3:] if l.strip()]
    return joints, frames

def positions(joints, frame):
    at, turn, k = [None] * len(joints), [None] * len(joints), 0
    for j, jo in enumerate(joints):
        local = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
        pos = jo['offset']
        for c in jo['channels']:
            v = frame[k]; k += 1
            if c.endswith('position'):
                pos = list(pos); pos['XYZ'.index(c[0])] = v
            else:
                local = mul(local, rot(c[0], v))
        if jo['parent'] < 0:
            at[j], turn[j] = pos, local
        else:
            p = jo['parent']
            at[j] = add(at[p], app(turn[p], pos))
            turn[j] = mul(turn[p], local)
    return at

PHASES = 32
NAMES = ['Hips', 'Chest', 'Chest4', 'Neck', 'Head', 'Head_end', 'RightShoulder', 'RightElbow', 'RightWrist', 'LeftShoulder',
         'LeftElbow', 'LeftWrist', 'RightHip', 'RightKnee', 'RightAnkle', 'RightToe', 'LeftHip', 'LeftKnee', 'LeftAnkle', 'LeftToe']

class Take:
    """A take read: each frame's joints (P, by idx), its frame rate, its heading each frame (yaw: radians, the way
    it faces), the length of its leg, and when each foot's down (feet: right, left)."""
    def __init__(self, path, start=0, stop=None):
        if path.endswith('.amc'):
            self.fps = 120.0
            subject = os.path.basename(path).split('_')[0]
            names_in, P = asfamc.read(os.path.join(os.path.dirname(path), subject + '.asf'), path)
            self.idx = {nm: k for k, nm in enumerate(names_in)}
        elif path.endswith('.fbx'):
            self.fps = fbx.FPS
            names_in, P = fbx.read(path)
            self.idx = {nm: k for k, nm in enumerate(names_in)}
        else:
            self.fps = 60.0
            joints, frames = read(path)
            self.idx = {j['name']: n for n, j in enumerate(joints)}
            P = [positions(joints, f) for f in frames]
        self.P = P = P[start:stop]
        idx, FPS = self.idx, self.fps
        self.n = n = len(P)
        # its heading: square to the hips, the way it faces, whichever way it goes (a take may go backward, or
        # sideways: sprint 105, it was taken as the way it went)
        head = []
        for p in P:
            across = sub(p[idx['LeftHip']], p[idx['RightHip']])  # to its left
            head.append(norm([-across[2], 0, across[0]]))       # left × up: ahead
        # smoothed, so a stride's sway isn't a turn
        W = int(FPS / 2)
        smooth = []
        for i in range(n):
            s_ = [0, 0, 0]
            for k in range(max(0, i - W), min(n, i + W + 1)):
                s_ = add(s_, head[k])
            smooth.append(norm([s_[0], 0, s_[2]]))
        self.yaw = [math.atan2(h[0], h[2]) for h in smooth]
        self.leg = leg = length(sub(P[0][idx['RightHip']], P[0][idx['RightKnee']])) + length(sub(P[0][idx['RightKnee']], P[0][idx['RightAnkle']]))
        # each foot's down when its heel or toe is low and still, and stays so a few frames (sprint 105: by the ankle's
        # height alone the strides cut were anything from a step to two, and averaged into mush)
        LOW, STILL, HOLDS = 0.046 * leg, 0.345 * leg, max(1, round(FPS / 20))  # over the lowest it goes, /s, frames (4 cm, 30 cm/s of 100STYLE's)
        def down(heel, toe):
            on = []
            for part in (heel, toe):
                ys = sorted(p[idx[part]][1] for p in P)
                floor = ys[len(ys) // 20]
                on.append([P[i][idx[part]][1] < floor + LOW and
                           FPS * math.hypot(*(sub(P[min(i + 1, n - 1)][idx[part]], P[max(i - 1, 0)][idx[part]])[c] / 2 for c in (0, 2))) < STILL
                           for i in range(n)])
            d = [on[0][i] or on[1][i] for i in range(n)]
            for want in (True, False):  # a flicker either way isn't a step
                i = 0
                while i < n:
                    k = i
                    while k < n and d[k] == d[i]:
                        k += 1
                    if d[i] == want and k - i < HOLDS and i > 0 and k < n:
                        for m in range(i, k):
                            d[m] = not want
                    i = k
            return d
        self.feet = [down('RightAnkle', 'RightToe'), down('LeftAnkle', 'LeftToe')]

    def frame(self, p, y):
        """A frame's joints (p: as P's, maybe between two) against its hips, in its own frame as one facing `y`:
        z ahead, x to its right (100STYLE's +x is its left), in legs; the hips' own, their height."""
        c_, s_ = math.cos(y), math.sin(y)
        hips = p[self.idx['Hips']]
        out = []
        for nm in NAMES:
            d = sub(p[self.idx[nm]], hips)
            ahead = d[0] * s_ + d[2] * c_
            left = d[0] * c_ - d[2] * s_
            out.append([-left / self.leg, (d[1] if nm != 'Hips' else hips[1]) / self.leg, ahead / self.leg])
        return out

def take(path, start, stop):
    """One take's straight strides: each a stride's phases, its end, its feet down, its pace (legs/s) and period (s)."""
    t = Take(path, start, stop)
    P, idx, FPS, n, yaw, leg, feet = t.P, t.idx, t.fps, t.n, t.yaw, t.leg, t.feet
    frame = t.frame
    keep = [i for i in range(1, n) if feet[0][i] and not feet[0][i - 1]]  # the right foot coming down
    names = NAMES
    pairs = [(a, b) for a, b in zip(keep, keep[1:]) if abs(math.remainder(yaw[b] - yaw[a], 2 * math.pi)) < math.radians(20)]
    strides = []
    for a, b in pairs:
        period = b - a
        went = sub(P[b][0], P[a][0])
        cyc, on = [], []
        for ph in range(PHASES):
            f = a + period * ph / PHASES
            i0 = int(f); t = f - i0
            p = [[P[i0][j][c] + (P[i0 + 1][j][c] - P[i0][j][c]) * t for c in range(3)] for j in range(len(P[0]))]
            cyc.append(frame(p, yaw[i0]))
            on.append([feet[side][round(f)] for side in (0, 1)])
        # which way it went, against the way it faces: degrees, 0 ahead, 90 to its right
        c_, s_ = math.cos(yaw[a]), math.sin(yaw[a])
        way = math.degrees(math.atan2(-(went[0] * c_ - went[2] * s_), went[0] * s_ + went[2] * c_))
        strides.append((cyc, frame(P[b], yaw[b]), on, math.hypot(went[0], went[2]) / (period / FPS) / leg, period / FPS, way))
    return strides, leg

def main():
    # several takes (a,b,c: CMU's are a stride or two each) pooled into one stride
    args = sys.argv[1:]
    want_way = None  # --way 90: only strides going that way against how it faces (degrees, 0 ahead, 90 its right)
    if '--way' in args:
        k = args.index('--way')
        want_way = float(args[k + 1])
        del args[k:k + 2]
    paths, name = args[0].split(','), args[1]
    start, stop = (int(args[2]), int(args[3])) if len(args) > 3 else (0, None)
    every, leg = [], 0
    for path in paths:
        strides, leg = take(path, start, stop)
        every += strides
    if want_way is not None:
        every = [st for st in every if abs(math.remainder(st[5] - want_way, 360)) < 45]
    if not every:
        sys.exit('no straight strides found')
    # all about as long as the rest: a stride that's a step, or two, isn't one
    usual = sorted(st[4] for st in every)[len(every) // 2]
    kept = [st for st in every if abs(st[4] - usual) <= 0.15 * usual]
    cycles, ends, contacts = [st[0] for st in kept], [st[1] for st in kept], [st[2] for st in kept]
    speeds, periods = [st[3] for st in kept], [st[4] for st in kept]
    names = NAMES
    J = len(names)
    mean = [[[sum(c[ph][j][k] for c in cycles) / len(cycles) for k in range(3)] for j in range(J)] for ph in range(PHASES)]
    # closed: what's left between its end and its start spread over the stride, so it runs on with no jump
    end = [[sum(e[j][k] for e in ends) / len(ends) for k in range(3)] for j in range(J)]
    for ph in range(PHASES):
        for j in range(J):
            for k in range(3):
                mean[ph][j][k] -= (end[j][k] - mean[0][j][k]) * ph / PHASES
    down_at = [[sum(c[ph][side] for c in contacts) * 2 > len(contacts) for side in (0, 1)] for ph in range(PHASES)]
    ways = [st[5] for st in kept]
    way = math.degrees(math.atan2(sum(math.sin(math.radians(w)) for w in ways), sum(math.cos(math.radians(w)) for w in ways)))
    json.dump({'name': name, 'joints': names, 'strides': len(cycles), 'pace': sum(speeds) / len(speeds), 'way': way,
               'period': sum(periods) / len(periods), 'leg_cm': leg, 'phases': mean, 'down': down_at}, sys.stdout)
    shown = ''.join('B' if d[0] and d[1] else 'R' if d[0] else 'L' if d[1] else '-' for d in down_at)
    print(f"{name}: {len(cycles)} strides of {len(every)} straight, {usual:.3f} s usual, pace {sum(speeds) / len(speeds):.2f} legs/s, "
          f"period {sum(periods) / len(periods):.3f} s, going {way:.0f} degrees, feet {shown}", file=sys.stderr)

if __name__ == '__main__':
    main()
