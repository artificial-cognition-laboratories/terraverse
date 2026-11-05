#!/usr/bin/env python3
"""
A stride cycle from a motion capture (sprint 105): where each joint is, through one stride, in the walker's own frame.

Reads a BVH (100STYLE's: cm, Y up, its left at +x), finds the stretches where it goes straight and steady, cuts them
at each right heel strike, and averages those strides into one. Out, per phase: each joint against the hips, in
lengths of its leg, x to its right, y up, z ahead.

  cycles.py Neutral_FW.bvh walk [start stop] > walk.json
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
    # right heel strikes: the ankle at its lowest, coming down, moving ahead little
    ay = [p[idx['RightAnkle']][1] for p in P]
    low = min(ay)
    strikes = [i for i in range(2, n - 2) if ay[i] < ay[i - 1] and ay[i] <= ay[i + 1] and ay[i] < low + 0.25 * (max(ay) - low)]
    # keep only strikes a stride apart (the lowest of each dip)
    keep = []
    for s in strikes:
        if keep and s - keep[-1] < 20:
            if ay[s] < ay[keep[-1]]:
                keep[-1] = s
            continue
        keep.append(s)
    PHASES = 32
    names = ['Hips', 'Chest', 'Chest4', 'Neck', 'Head', 'Head_end', 'RightShoulder', 'RightElbow', 'RightWrist', 'LeftShoulder',
             'LeftElbow', 'LeftWrist', 'RightHip', 'RightKnee', 'RightAnkle', 'RightToe', 'LeftHip', 'LeftKnee', 'LeftAnkle', 'LeftToe']
    cycles, speeds, periods = [], [], []
    for a, b in zip(keep, keep[1:]):
        period = b - a
        turned = abs(math.remainder(yaw[b] - yaw[a], 2 * math.pi))
        went = sub(P[b][0], P[a][0])
        speed = math.hypot(went[0], went[2]) / (period / 60.0)
        if period < 30 or period > 90 or turned > math.radians(20):
            continue
        cyc = []
        for ph in range(PHASES):
            f = a + period * ph / PHASES
            i0 = int(f); t = f - i0
            p = [[P[i0][j][c] + (P[i0 + 1][j][c] - P[i0][j][c]) * t for c in range(3)] for j in range(len(joints))]
            y = yaw[i0]
            c_, s_ = math.cos(y), math.sin(y)
            hips = p[idx['Hips']]
            out = []
            for nm in names:
                d = sub(p[idx[nm]], hips)
                # into its own frame: z ahead, x to its right (100STYLE's +x is its left)
                ahead = d[0] * s_ + d[2] * c_
                left = d[0] * c_ - d[2] * s_
                out.append([-left / leg, (d[1] if nm != 'Hips' else p[idx['Hips']][1]) / leg, ahead / leg])
            cyc.append(out)
        cycles.append(cyc); speeds.append(speed / leg); periods.append(period / 60.0)
    if not cycles:
        sys.exit('no straight strides found')
    mean = [[[sum(c[ph][j][k] for c in cycles) / len(cycles) for k in range(3)] for j in range(len(names))] for ph in range(PHASES)]
    json.dump({'name': name, 'joints': names, 'strides': len(cycles), 'pace': sum(speeds) / len(speeds),
               'period': sum(periods) / len(periods), 'leg_cm': leg, 'phases': mean}, sys.stdout)
    print(f"{name}: {len(cycles)} strides of {len(keep) - 1}, pace {sum(speeds) / len(speeds):.2f} legs/s, period {sum(periods) / len(periods):.3f} s", file=sys.stderr)

main()
