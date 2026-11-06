#!/usr/bin/env python3
"""
A stride cycle from a motion capture (sprint 105): where each joint is, through one stride, in the walker's own frame.

Reads a BVH (100STYLE's: cm, Y up, its left at +x, 60 frames a second), finds when each foot's on the ground (low
and still, heel or toe), cuts strides at each right foot coming down, keeps those going straight that take about as
long as the rest, and averages them into one, closed so its end runs on into its start. Out, per phase: each joint
against the hips, in lengths of its leg, x to its right, y up, z ahead; and whether each foot's down, in most strides.

  cycles.py Neutral_FW.bvh walk 327 7445 > walk.json   (100STYLE's Frame_Cuts: its forward walk)
  cycles.py Neutral_FR.bvh run 357 4227 > run.json
"""
import json, math, sys

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

def main():
    path, name = sys.argv[1], sys.argv[2]
    start, stop = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (0, None)
    joints, frames = read(path)
    frames = frames[start:stop]
    idx = {j['name']: n for n, j in enumerate(joints)}
    P = [positions(joints, f) for f in frames]
    n = len(P)
    # its heading: square to the hips, toward where it goes
    head = []
    for p in P:
        across = sub(p[idx['LeftHip']], p[idx['RightHip']])  # to its left
        fwd = norm([across[2], 0, -across[0]])               # left × up... checked against the going below
        head.append(fwd)
    vel = [sub(P[min(i + 1, n - 1)][0], P[max(i - 1, 0)][0]) for i in range(n)]
    if sum(head[i][0] * vel[i][0] + head[i][2] * vel[i][2] for i in range(n)) < 0:
        head = [[-h[0], 0, -h[2]] for h in head]
    # smoothed, so a stride's sway isn't a turn
    W = 30
    smooth = []
    for i in range(n):
        s = [0, 0, 0]
        for k in range(max(0, i - W), min(n, i + W + 1)):
            s = add(s, head[k])
        smooth.append(norm([s[0], 0, s[2]]))
    yaw = [math.atan2(h[0], h[2]) for h in smooth]
    leg = length(sub(P[0][idx['RightHip']], P[0][idx['RightKnee']])) + length(sub(P[0][idx['RightKnee']], P[0][idx['RightAnkle']]))
    # each foot's down when its heel or toe is low and still, and stays so a few frames (sprint 105: by the ankle's
    # height alone the strides cut were anything from a step to two, and averaged into mush)
    LOW, STILL, HOLDS = 4.0, 30.0, 3  # cm over the lowest it goes, cm/s, frames
    def down(heel, toe):
        on = []
        for part in (heel, toe):
            ys = sorted(p[idx[part]][1] for p in P)
            floor = ys[len(ys) // 20]
            on.append([P[i][idx[part]][1] < floor + LOW and
                       60.0 * math.hypot(*(sub(P[min(i + 1, n - 1)][idx[part]], P[max(i - 1, 0)][idx[part]])[c] / 2 for c in (0, 2))) < STILL
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
    feet = [down('RightAnkle', 'RightToe'), down('LeftAnkle', 'LeftToe')]
    keep = [i for i in range(1, n) if feet[0][i] and not feet[0][i - 1]]  # the right foot coming down
    PHASES = 32
    names = ['Hips', 'Chest', 'Chest4', 'Neck', 'Head', 'Head_end', 'RightShoulder', 'RightElbow', 'RightWrist', 'LeftShoulder',
             'LeftElbow', 'LeftWrist', 'RightHip', 'RightKnee', 'RightAnkle', 'RightToe', 'LeftHip', 'LeftKnee', 'LeftAnkle', 'LeftToe']
    def frame(p, y):
        c_, s_ = math.cos(y), math.sin(y)
        hips = p[idx['Hips']]
        out = []
        for nm in names:
            d = sub(p[idx[nm]], hips)
            # into its own frame: z ahead, x to its right (100STYLE's +x is its left)
            ahead = d[0] * s_ + d[2] * c_
            left = d[0] * c_ - d[2] * s_
            out.append([-left / leg, (d[1] if nm != 'Hips' else hips[1]) / leg, ahead / leg])
        return out
    pairs = [(a, b) for a, b in zip(keep, keep[1:]) if abs(math.remainder(yaw[b] - yaw[a], 2 * math.pi)) < math.radians(20)]
    usual = sorted(b - a for a, b in pairs)[len(pairs) // 2]
    cycles, ends, contacts, speeds, periods = [], [], [], [], []
    for a, b in pairs:
        period = b - a
        if abs(period - usual) > 0.15 * usual:
            continue
        went = sub(P[b][0], P[a][0])
        cyc, on = [], []
        for ph in range(PHASES):
            f = a + period * ph / PHASES
            i0 = int(f); t = f - i0
            p = [[P[i0][j][c] + (P[i0 + 1][j][c] - P[i0][j][c]) * t for c in range(3)] for j in range(len(joints))]
            cyc.append(frame(p, yaw[i0]))
            on.append([feet[side][round(f)] for side in (0, 1)])
        cycles.append(cyc); ends.append(frame(P[b], yaw[b])); contacts.append(on)
        speeds.append(math.hypot(went[0], went[2]) / (period / 60.0) / leg); periods.append(period / 60.0)
    if not cycles:
        sys.exit('no straight strides found')
    J = len(names)
    mean = [[[sum(c[ph][j][k] for c in cycles) / len(cycles) for k in range(3)] for j in range(J)] for ph in range(PHASES)]
    # closed: what's left between its end and its start spread over the stride, so it runs on with no jump
    end = [[sum(e[j][k] for e in ends) / len(ends) for k in range(3)] for j in range(J)]
    for ph in range(PHASES):
        for j in range(J):
            for k in range(3):
                mean[ph][j][k] -= (end[j][k] - mean[0][j][k]) * ph / PHASES
    down_at = [[sum(c[ph][side] for c in contacts) * 2 > len(contacts) for side in (0, 1)] for ph in range(PHASES)]
    json.dump({'name': name, 'joints': names, 'strides': len(cycles), 'pace': sum(speeds) / len(speeds),
               'period': sum(periods) / len(periods), 'leg_cm': leg, 'phases': mean, 'down': down_at}, sys.stdout)
    shown = ''.join('B' if d[0] and d[1] else 'R' if d[0] else 'L' if d[1] else '-' for d in down_at)
    print(f"{name}: {len(cycles)} strides of {len(pairs)} straight, {usual} frames usual, pace {sum(speeds) / len(speeds):.2f} legs/s, "
          f"period {sum(periods) / len(periods):.3f} s, feet {shown}", file=sys.stderr)

main()
