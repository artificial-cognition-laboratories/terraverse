"""
Mixamo's motions (mixamo.com: free to use in games, an Adobe login to download) as cycles.py reads a BVH (sprint
105): each frame, where each joint is, named as 100STYLE's are. A binary .fbx, downloaded without skin; read here
without the FBX SDK, sampled 60 frames a second; lengths in its own units (cycles.py measures in legs).

As the FBX SDK has it: each bone's placed at its Lcl Translation from its parent, turned by its PreRotation, then
its Lcl Rotation, then back by its PostRotation (each x first, then y, then z, about fixed axes); a key's time in
ticks, 46186158000 a second.
"""
import math, struct, zlib
from asfamc import euler, mul, app, tr

TICKS, FPS = 46186158000, 60.0

# Each of 100STYLE's joints, as a Mixamo bone (where it starts).
BONES = {
    'Hips': 'Hips', 'Chest': 'Spine', 'Chest4': 'Spine2', 'Neck': 'Neck', 'Head': 'Head', 'Head_end': 'HeadTop_End',
    'RightShoulder': 'RightArm', 'RightElbow': 'RightForeArm', 'RightWrist': 'RightHand',
    'LeftShoulder': 'LeftArm', 'LeftElbow': 'LeftForeArm', 'LeftWrist': 'LeftHand',
    'RightHip': 'RightUpLeg', 'RightKnee': 'RightLeg', 'RightAnkle': 'RightFoot', 'RightToe': 'RightToeBase',
    'LeftHip': 'LeftUpLeg', 'LeftKnee': 'LeftLeg', 'LeftAnkle': 'LeftFoot', 'LeftToe': 'LeftToeBase',
}

def parse(data):
    """The file's nodes, each (name, properties, children)."""
    if not data.startswith(b'Kaydara FBX Binary  \x00'):
        raise ValueError('not a binary FBX (download it as FBX Binary)')
    version = struct.unpack_from('<I', data, 23)[0]
    wide = version >= 7500
    head, size = ('<QQQ', 25) if wide else ('<III', 13)
    def prop(at):
        t = chr(data[at]); at += 1
        scalar = {'Y': '<h', 'C': '<?', 'I': '<i', 'F': '<f', 'D': '<d', 'L': '<q'}
        if t in scalar:
            v = struct.unpack_from(scalar[t], data, at)[0]
            return v, at + struct.calcsize(scalar[t])
        if t in 'fdlib':
            n, enc, clen = struct.unpack_from('<III', data, at); at += 12
            raw = data[at:at + clen]; at += clen
            if enc:
                raw = zlib.decompress(raw)
            code = {'f': 'f', 'd': 'd', 'l': 'q', 'i': 'i', 'b': '?'}[t]
            return list(struct.unpack('<%d%s' % (n, code), raw)), at
        if t in 'SR':
            n = struct.unpack_from('<I', data, at)[0]; at += 4
            v = data[at:at + n]
            return (v.decode('utf-8', 'replace') if t == 'S' else v), at + n
        raise ValueError('unknown FBX property type ' + t)
    def node(at):
        end, nprops, _ = struct.unpack_from(head, data, at); at += size - 1
        if end == 0:
            return None, at + 1
        n = data[at]; name = data[at + 1:at + 1 + n].decode(); at += 1 + n
        props = []
        for _ in range(nprops):
            v, at = prop(at)
            props.append(v)
        kids = []
        while at < end:
            k, at = node(at)
            if k is None:
                break
            kids.append(k)
        return (name, props, kids), end
    out, at = [], 27
    while at < len(data) - size:
        k, at = node(at)
        if k is None:
            break
        out.append(k)
    return out

def child(n, name):
    return next((k for k in n[2] if k[0] == name), None)

def props70(n):
    """A node's Properties70, by name: its values."""
    p = child(n, 'Properties70')
    return {k[1][0]: k[1][4:] for k in p[2]} if p else {}

def read(path):
    """(names, frames): each frame, each name's place, as [x, y, z]."""
    top = parse(open(path, 'rb').read())
    objects = next(n for n in top if n[0] == 'Objects')
    links = next(n for n in top if n[0] == 'Connections')
    models, curve_nodes, curves = {}, {}, {}
    for o in objects[2]:
        oid = o[1][0]
        if o[0] == 'Model':
            nm = o[1][1].split('\x00')[0].split(':')[-1]
            models[oid] = {'name': nm, 'p': props70(o), 'parent': None, 'anim': {}}
        elif o[0] == 'AnimationCurveNode':
            curve_nodes[oid] = {'p': props70(o), 'axes': {}}
        elif o[0] == 'AnimationCurve':
            curves[oid] = (child(o, 'KeyTime')[1][0], child(o, 'KeyValueFloat')[1][0])
    for c in links[2]:
        kind, a, b = c[1][0], c[1][1], c[1][2]
        if kind == 'OO' and a in models and b in models:
            models[a]['parent'] = b
        elif kind == 'OP' and a in curve_nodes and b in models:
            models[b]['anim'][c[1][3]] = curve_nodes[a]
        elif kind == 'OP' and a in curves and b in curve_nodes:
            curve_nodes[b]['axes'][c[1][3]] = curves[a]
    by_name = {m['name']: m for m in models.values()}
    missing = [b for b in BONES.values() if b not in by_name]
    if missing:
        raise ValueError('not a Mixamo skeleton: no ' + ', '.join(missing))
    stop = max((t[-1] for t, _ in curves.values()), default=0) / TICKS

    def value(m, prop, at):
        """A model's prop at a time (s): its curves, or else what it's set to."""
        rest = list(m['p'].get(prop, (0.0, 0.0, 0.0)))
        cn = m['anim'].get(prop)
        if not cn:
            return rest
        out = []
        for k, axis in enumerate(('d|X', 'd|Y', 'd|Z')):
            if axis not in cn['axes']:
                out.append(cn['p'].get(axis, (rest[k],))[0])
                continue
            ts, vs = cn['axes'][axis]
            tick = at * TICKS
            if tick <= ts[0]:
                out.append(vs[0]); continue
            if tick >= ts[-1]:
                out.append(vs[-1]); continue
            lo, hi = 0, len(ts) - 1
            while hi - lo > 1:
                mid = (lo + hi) // 2
                lo, hi = (mid, hi) if ts[mid] <= tick else (lo, mid)
            u = (tick - ts[lo]) / (ts[hi] - ts[lo])
            out.append(vs[lo] + (vs[hi] - vs[lo]) * u)
        return out

    names = list(BONES)
    frames = []
    for f in range(int(stop * FPS) + 1):
        at = f / FPS
        place, turn = {}, {}
        def solve(oid):
            if oid in place:
                return
            m = models[oid]
            local = mul(euler(m['p'].get('PreRotation', (0, 0, 0))),
                        mul(euler(value(m, 'Lcl Rotation', at)), tr(euler(m['p'].get('PostRotation', (0, 0, 0))))))
            t = value(m, 'Lcl Translation', at)
            p = m['parent']
            if p is None:
                place[oid], turn[oid] = t, local
                return
            solve(p)
            reach = app(turn[p], t)
            place[oid] = [place[p][k] + reach[k] for k in range(3)]
            turn[oid] = mul(turn[p], local)
        row = []
        for nm in names:
            oid = next(k for k, m in models.items() if m['name'] == BONES[nm])
            solve(oid)
            row.append(place[oid])
        frames.append(row)
    return names, frames
