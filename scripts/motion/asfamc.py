"""
CMU's motion capture (mocap.cs.cmu.edu: free for all uses, not to be resold as data) as cycles.py reads a BVH
(sprint 105): each frame, where each joint is, named as 100STYLE's are. An .asf is a subject's skeleton, an .amc a
take's joint angles, 120 frames a second; lengths in their own units (cycles.py measures in legs).

As CMU's own viewer has it: each bone turns in its own axes (`axis`, x then y then z), against its parent's, and
reaches `length` along `direction` from where its parent ends.
"""
import math

def rot(axis, deg):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    if axis == 'x': return [[1, 0, 0], [0, c, -s], [0, s, c]]
    if axis == 'y': return [[c, 0, s], [0, 1, 0], [-s, 0, c]]
    return [[c, -s, 0], [s, c, 0], [0, 0, 1]]

def mul(a, b): return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def app(m, v): return [sum(m[i][k] * v[k] for k in range(3)) for i in range(3)]
def tr(m): return [[m[j][i] for j in range(3)] for i in range(3)]

def euler(xyz):  # x first, then y, then z, about fixed axes
    return mul(rot('z', xyz[2]), mul(rot('y', xyz[1]), rot('x', xyz[0])))

def read_asf(path):
    bones, parents, order = {}, {}, []
    section, bone = None, None
    root_order = ['TX', 'TY', 'TZ', 'RX', 'RY', 'RZ']
    lines = open(path).read().split('\n')
    i = 0
    while i < len(lines):
        t = lines[i].split()
        i += 1
        if not t:
            continue
        if t[0].startswith(':'):
            section = t[0]
            continue
        if section == ':root' and t[0] == 'order':
            root_order = [x.upper() for x in t[1:]]
        elif section == ':bonedata':
            if t[0] == 'begin':
                bone = {'dof': []}
            elif t[0] == 'name':
                bone['name'] = t[1]
            elif t[0] == 'direction':
                bone['direction'] = list(map(float, t[1:4]))
            elif t[0] == 'length':
                bone['length'] = float(t[1])
            elif t[0] == 'axis':
                bone['axis'] = euler(list(map(float, t[1:4])))
            elif t[0] == 'dof':
                bone['dof'] = t[1:]
            elif t[0] == 'end':
                bones[bone['name']] = bone
                order.append(bone['name'])
        elif section == ':hierarchy' and t[0] not in ('begin', 'end'):
            for child in t[1:]:
                parents[child] = t[0]
    return bones, parents, root_order

def read_amc(path):
    frames, frame = [], None
    for line in open(path):
        t = line.split()
        if not t or t[0].startswith('#') or t[0].startswith(':'):
            continue
        if len(t) == 1 and t[0].isdigit():
            frame = {}
            frames.append(frame)
        elif frame is not None:
            frame[t[0]] = list(map(float, t[1:]))
    return frames

# Each of 100STYLE's joints, as where a CMU bone ends (the hips: the root itself).
ENDS = {
    'Chest': 'lowerback', 'Chest4': 'upperback', 'Neck': 'thorax', 'Head': 'upperneck', 'Head_end': 'head',
    'RightShoulder': 'rclavicle', 'RightElbow': 'rhumerus', 'RightWrist': 'rradius',
    'LeftShoulder': 'lclavicle', 'LeftElbow': 'lhumerus', 'LeftWrist': 'lradius',
    'RightHip': 'rhipjoint', 'RightKnee': 'rfemur', 'RightAnkle': 'rtibia', 'RightToe': 'rfoot',
    'LeftHip': 'lhipjoint', 'LeftKnee': 'lfemur', 'LeftAnkle': 'ltibia', 'LeftToe': 'lfoot',
}

def read(asf, amc):
    """(names, frames): each frame, each name's place, as [x, y, z]."""
    bones, parents, root_order = read_asf(asf)
    names = ['Hips'] + list(ENDS)
    out = []
    for f in read_amc(amc):
        rv = f['root']
        get = {c: rv[k] for k, c in enumerate(root_order)}
        place = {'root': [get.get('TX', 0), get.get('TY', 0), get.get('TZ', 0)]}
        turn = {'root': euler([get.get('RX', 0), get.get('RY', 0), get.get('RZ', 0)])}
        def solve(name):
            if name in place:
                return
            p = parents[name]
            solve(p)
            b = bones[name]
            angles = {'rx': 0.0, 'ry': 0.0, 'rz': 0.0}
            for k, d in enumerate(b['dof']):
                angles[d] = f.get(name, [0.0] * len(b['dof']))[k]
            local = mul(b['axis'], mul(euler([angles['rx'], angles['ry'], angles['rz']]), tr(b['axis'])))
            turn[name] = mul(turn[p], local)
            reach = app(turn[name], [x * b['length'] for x in b['direction']])
            place[name] = [place[p][k] + reach[k] for k in range(3)]
        row = [place['root']]
        for nm in ENDS.values():
            solve(nm)
            row.append(place[nm])
        out.append(row)
    return names, out
