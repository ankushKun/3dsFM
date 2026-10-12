#!/usr/bin/env python3
"""Builds 3dsFM's 3D HOME Menu banner as a binary glTF (banner.glb).

The banner is what the 3DS shows on the top screen while 3dsFM is selected
on the HOME Menu: a glossy dark record with a Clearwater-blue label, swaying
and spinning, a tonearm that swings in and drops its needle, a happy squash
and a burst of music notes, with the 3dsFM wordmark floating in front. Every shape is generated here, so the model can
be reviewed and changed as code, and needs no 3D editor.

Units follow the HOME Menu banner camera (pycgfx's banner-camera.gltf): the
camera sits at (0, 1, 44.786) looking down -z with a 30 degree vertical
field of view, so the plane z = 0 shows x from -20 to 20 and y from -11 to
13, and one unit is ten pixels of the 400 x 240 top screen. Things with a
larger z sit in front of the screen when the 3D slider is up.

Only rigid node animation (translation, rotation and scale keys) is used:
skinned animation has been reported to freeze the HOME Menu on real
hardware. Nothing is ever scaled to (nearly) nothing: parts that should
vanish hide behind the record instead. The wordmark is a hard-edged cut-out
(alpha MASK), not a blended transparency. The loop is four seconds and ends
where it starts.

Colours are given as the 3DS will show them (pycgfx passes them to the GPU
unchanged), not as linear values.

Usage: python3 model.py logo.png banner.glb [--static SECONDS]

--static writes the scene as it is at that moment, with no animation.
"""

import json
import math
import struct
import sys

LOOP = 4.0  # seconds


# --------------------------------------------------------------- 2D shapes


def norm2(x, y):
    length = math.hypot(x, y)
    return (x / length, y / length) if length else (0.0, 0.0)


def rounded(points, radius, segments=6):
    """Rounds every corner of a polygon with an arc of the given radius.

    radius is one number or one per corner. Works for convex and concave
    corners alike (a concave corner gets a fillet)."""
    radii = radius if isinstance(radius, (list, tuple)) else [radius] * len(points)
    out = []
    n = len(points)
    for i, (px, py) in enumerate(points):
        r = radii[i]
        if r <= 0:
            out.append((px, py))
            continue
        ax, ay = points[i - 1]
        bx, by = points[(i + 1) % n]
        ux, uy = norm2(ax - px, ay - py)
        vx, vy = norm2(bx - px, by - py)
        theta = math.acos(max(-1.0, min(1.0, ux * vx + uy * vy)))
        t = r / math.tan(theta / 2)
        sx, sy = px + ux * t, py + uy * t
        ex, ey = px + vx * t, py + vy * t
        bx_, by_ = norm2(ux + vx, uy + vy)
        d = r / math.sin(theta / 2)
        cx, cy = px + bx_ * d, py + by_ * d
        a0 = math.atan2(sy - cy, sx - cx)
        a1 = math.atan2(ey - cy, ex - cx)
        sweep = (a1 - a0 + math.pi) % (2 * math.pi) - math.pi
        for k in range(segments + 1):
            a = a0 + sweep * k / segments
            out.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return out


def signed_area(poly):
    return 0.5 * sum(
        poly[i - 1][0] * poly[i][1] - poly[i][0] * poly[i - 1][1] for i in range(len(poly))
    )


def triangulate(poly):
    """Ear clipping for a simple counter-clockwise polygon."""

    def cross(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])

    def inside(p, a, b, c):
        return cross(a, b, p) >= 0 and cross(b, c, p) >= 0 and cross(c, a, p) >= 0

    idx = list(range(len(poly)))
    tris = []
    while len(idx) > 3:
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = poly[i0], poly[i1], poly[i2]
            turn = cross(a, b, c)
            if abs(turn) < 1e-9:
                idx.pop(k)  # collinear: drop the middle point
                break
            if turn < 0:
                continue  # reflex corner
            if any(inside(poly[j], a, b, c) for j in idx if j not in (i0, i1, i2)):
                continue
            tris.append((i0, i1, i2))
            idx.pop(k)
            break
        else:
            raise RuntimeError("could not triangulate the outline (is a corner rounded less than its bevel?)")
    tris.append(tuple(idx))
    return tris


def miters(poly):
    """Per-corner outward direction and the length factor that keeps an
    offset edge parallel to the original."""
    out = []
    n = len(poly)
    for i in range(n):
        (ax, ay), (px, py), (bx, by) = poly[i - 1], poly[i], poly[(i + 1) % n]
        n1 = norm2(py - ay, -(px - ax))
        n2 = norm2(by - py, -(bx - px))
        mx, my = norm2(n1[0] + n2[0], n1[1] + n2[1])
        scale = 1 / max(0.35, mx * n1[0] + my * n1[1])
        out.append((mx, my, scale))
    return out


# --------------------------------------------------------------- 3D meshes


class Mesh:
    def __init__(self):
        self.positions = []
        self.normals = []
        self.uvs = []
        self.indices = []

    def vertex(self, p, n, uv=None):
        self.positions.append(p)
        self.normals.append(n)
        if uv is not None:
            self.uvs.append(uv)
        return len(self.positions) - 1


def norm3(x, y, z):
    length = math.sqrt(x * x + y * y + z * z)
    return (x / length, y / length, z / length)


def extrude(outline, depth, bevel):
    """A chunky solid: the outline extruded to depth, with a rounded-looking
    bevel around the front and back faces and smooth side normals.

    Convex corners need rounding at least as large as the bevel, or the
    inset face folds over itself and cannot be triangulated."""
    if signed_area(outline) < 0:
        outline = outline[::-1]
    m = miters(outline)
    inset = [(x - mx * bevel * s, y - my * bevel * s) for (x, y), (mx, my, s) in zip(outline, m)]
    h = depth / 2
    mesh = Mesh()
    n = len(outline)

    def loop(points, z, normal_of):
        return [mesh.vertex((x, y, z), normal_of(i)) for i, (x, y) in enumerate(points)]

    def band(top, bottom):
        for i in range(n):
            j = (i + 1) % n
            mesh.indices += [top[i], bottom[i], bottom[j], top[i], bottom[j], top[j]]

    # Front and back faces.
    front = loop(inset, h, lambda i: (0.0, 0.0, 1.0))
    back = loop(inset, -h, lambda i: (0.0, 0.0, -1.0))
    for a, b, c in triangulate(inset):
        mesh.indices += [front[a], front[b], front[c]]
        mesh.indices += [back[a], back[c], back[b]]

    # Bevels: normals swing from the face to the side, so highlights roll
    # round the edge like moulded plastic.
    f_in = loop(inset, h, lambda i: norm3(m[i][0] * 0.45, m[i][1] * 0.45, 1))
    f_out = loop(outline, h - bevel, lambda i: norm3(m[i][0], m[i][1], 0.3))
    band(f_in, f_out)
    s_top = loop(outline, h - bevel, lambda i: (m[i][0], m[i][1], 0.0))
    s_bot = loop(outline, -(h - bevel), lambda i: (m[i][0], m[i][1], 0.0))
    band(s_top, s_bot)
    b_out = loop(outline, -(h - bevel), lambda i: norm3(m[i][0], m[i][1], -0.3))
    b_in = loop(inset, -h, lambda i: norm3(m[i][0] * 0.45, m[i][1] * 0.45, -1))
    band(b_out, b_in)
    return mesh


def plane(width, height):
    """A camera-facing rectangle with texture coordinates (glTF puts the
    image's top-left at 0, 0)."""
    mesh = Mesh()
    w, h = width / 2, height / 2
    z = (0.0, 0.0, 1.0)
    a = mesh.vertex((-w, -h, 0.0), z, (0.0, 1.0))
    b = mesh.vertex((w, -h, 0.0), z, (1.0, 1.0))
    c = mesh.vertex((w, h, 0.0), z, (1.0, 0.0))
    d = mesh.vertex((-w, h, 0.0), z, (0.0, 0.0))
    mesh.indices += [a, b, c, a, c, d]
    return mesh


def merge(*meshes_and_offsets):
    out = Mesh()
    for mesh, (ox, oy, oz) in meshes_and_offsets:
        base = len(out.positions)
        out.positions += [(x + ox, y + oy, z + oz) for x, y, z in mesh.positions]
        out.normals += mesh.normals
        out.indices += [i + base for i in mesh.indices]
    return out


# ------------------------------------------------------------------ Shapes


def disc(radius, segments=32):
    return [(radius * math.cos(2 * math.pi * k / segments), radius * math.sin(2 * math.pi * k / segments)) for k in range(segments)]


def record():
    """The vinyl: a thick dark disc."""
    return merge((extrude(disc(5.6, 28), 0.9, 0.22), (0.0, 0.0, 0.0)))


def groove():
    """A lighter thin disc under a dark one reads as a groove ring; rings as
    single polygons would fold under the bevel inset."""
    return merge((extrude(disc(4.75, 16), 0.12, 0.03), (0.0, 0.0, 0.49)))


def groove_fill():
    return merge((extrude(disc(4.45, 16), 0.12, 0.03), (0.0, 0.0, 0.53)))


def label():
    return merge((extrude(disc(2.3, 16), 0.3, 0.1), (0.0, 0.0, 0.62)))


def label_ring():
    return merge((extrude(disc(1.55, 10), 0.12, 0.03), (0.0, 0.0, 0.82)))


def label_fill():
    return merge((extrude(disc(1.3, 10), 0.12, 0.03), (0.0, 0.0, 0.86)))


def spindle():
    return merge((extrude(disc(0.34, 8), 0.5, 0.1), (0.0, 0.0, 0.95)))


def arm():
    """The tonearm, built around its pivot at the origin: a round base, the
    arm reaching down-left to the needle, and the head."""
    base = extrude(disc(1.25, 12), 1.2, 0.3)
    shaft = rounded([(-0.5, -7.4), (0.5, -7.4), (0.5, 0.6), (-0.5, 0.6)], 0.3, 1)
    head = rounded([(-1.1, -9.2), (1.1, -9.2), (1.1, -7.0), (-1.1, -7.0)], 0.45, 2)
    needle = rounded([(-0.2, -9.9), (0.2, -9.9), (0.2, -9.0), (-0.2, -9.0)], 0.12, 1)
    return merge(
        (base, (0.0, 0.0, 0.3)),
        (extrude(shaft, 0.7, 0.15), (0.0, 0.0, 0.5)),
        (extrude(head, 1.0, 0.3), (0.0, 0.0, 0.45)),
        (extrude(needle, 0.4, 0.08), (0.0, 0.0, 0.0)),
    )


def note():
    """An eighth note: head, stem and a flag, as one solid."""
    head = [(0.95 * math.cos(a) - 0.0, 0.62 * math.sin(a) - 0.0) for a in [2 * math.pi * k / 8 + 0.5 for k in range(8)]]
    stem = rounded([(0.55, -0.1), (1.05, -0.1), (1.05, 3.2), (0.55, 3.2)], 0.22, 2)
    flag = rounded([(0.75, 3.2), (1.35, 3.2), (2.55, 1.9), (2.55, 1.0), (1.3, 2.25), (0.75, 2.4)], [0.18, 0.3, 0.3, 0.4, 0.3, 0.2], 2)
    return merge(
        (extrude(head, 0.6, 0.16), (0.0, 0.0, 0.0)),
        (extrude(stem, 0.5, 0.14), (0.0, 0.0, 0.0)),
        (extrude(flag, 0.5, 0.14), (0.0, 0.0, 0.0)),
    )


def star(outer, inner, points=5):
    corners = []
    for k in range(points * 2):
        r = outer if k % 2 == 0 else inner
        a = math.pi / 2 + k * math.pi / points
        corners.append((r * math.cos(a), r * math.sin(a)))
    return rounded(corners, [outer * 0.2 if k % 2 == 0 else 0 for k in range(points * 2)], 1)


# --------------------------------------------------------------- Animation


def euler_quat(x, y, z):
    """Quaternion for rotations about x, then y, then z (radians)."""
    cx, sx = math.cos(x / 2), math.sin(x / 2)
    cy, sy = math.cos(y / 2), math.sin(y / 2)
    cz, sz = math.cos(z / 2), math.sin(z / 2)
    return (
        sx * cy * cz - cx * sy * sz,
        cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz,
    )


def sampled(fn, step=0.1):
    """Keys every step seconds over the whole loop, from a function of time."""
    count = int(round(LOOP / step))
    return [(k * step, fn(k * step)) for k in range(count + 1)]


def wave(t, period=LOOP, phase=0.0):
    return math.sin(2 * math.pi * t / period + phase)




def record_tracks():
    """Sways from side to side, spins two turns per loop, and squashes
    happily when the needle lands."""
    rotation = sampled(lambda t: euler_quat(math.radians(-8), math.radians(16) * wave(t), math.radians(360.0 * t / 2.0)), 0.05)
    scale = [
        (0.0, (1, 1, 1)), (0.7, (1, 1, 1)), (0.8, (1.07, 0.92, 1.07)), (0.95, (0.95, 1.06, 0.95)),
        (1.1, (1.02, 0.98, 1.02)), (1.25, (1, 1, 1)), (LOOP, (1, 1, 1)),
    ]
    translation = sampled(lambda t: (0.0, 0.2 * wave(t, phase=math.pi / 2), 0.0))
    return translation, rotation, scale


def rig_tracks():
    """The sway, kept separate from the spin so the tilt reads as a tilt."""
    rotation = sampled(lambda t: euler_quat(math.radians(-8), math.radians(16) * wave(t), math.radians(2) * wave(t, phase=1.2)))
    return None, rotation, None


def arm_track():
    """Swings in from the side, lands the needle on the record with a small
    bounce at 0.7 s, rests, then lifts away before the loop ends."""
    angles = [
        (0.0, 52.0), (0.3, 52.0), (0.55, 10.0), (0.7, 0.0), (0.78, 4.0), (0.88, 0.0),
        (3.55, 0.0), (3.9, 52.0), (LOOP, 52.0),
    ]
    rotation = [(t, euler_quat(0, 0, math.radians(a))) for t, a in angles]
    return None, rotation, None


def note_tracks(start, end, size, delay, spin):
    """Pops out from under the needle, floats up and away while turning,
    shrinks a little, then waits tucked behind the record until the next
    landing."""
    t0 = 0.95 + delay
    tucked = (0.0, 0.0, -1.4)
    small = (0.35,) * 3
    translation = [(0.0, tucked), (t0, tucked), (t0 + 0.12, start), (t0 + 0.75, end), (t0 + 0.9, end), (t0 + 0.91, tucked), (LOOP, tucked)]
    scale = [(0.0, small), (t0, small), (t0 + 0.18, (size * 1.15,) * 3), (t0 + 0.6, (size,) * 3), (t0 + 0.9, (0.3,) * 3), (t0 + 0.91, small), (LOOP, small)]
    rotation = [(0.0, euler_quat(0, 0, 0)), (t0, euler_quat(0, 0, math.radians(-spin * 0.3))), (t0 + 0.9, euler_quat(0, 0, math.radians(spin))), (t0 + 0.91, euler_quat(0, 0, 0)), (LOOP, euler_quat(0, 0, 0))]
    return translation, rotation, scale


def twinkle_tracks(position, size, phase):
    scale = sampled(lambda t: (size * (0.8 + 0.25 * wave(t, LOOP / 2, phase)),) * 3)
    rotation = sampled(lambda t: euler_quat(0, 0, math.radians(14) * wave(t, LOOP, phase)))
    return [(0.0, position), (LOOP, position)], rotation, scale


def logo_tracks():
    """Bobs gently. It never turns: build-cgfx.py makes it a billboard, so
    the 3DS keeps it facing the viewer."""
    translation = sampled(lambda t: (8.6, 1.0 + 0.35 * wave(t, phase=math.pi / 2), 2.4))
    return translation, None, None


# ---------------------------------------------------------------- glTF out


def sample(keys, t):
    """A track's value at time t, interpolated linearly (rotations are
    normalised after blending, which is close enough between dense keys)."""
    if t <= keys[0][0]:
        return keys[0][1]
    for (t0, a), (t1, b) in zip(keys, keys[1:]):
        if t0 <= t <= t1:
            f = (t - t0) / (t1 - t0) if t1 > t0 else 0.0
            if len(a) == 4 and sum(x * y for x, y in zip(a, b)) < 0:
                b = tuple(-x for x in b)
            v = tuple(x + (y - x) * f for x, y in zip(a, b))
            if len(v) == 4:
                n = math.sqrt(sum(x * x for x in v))
                v = tuple(x / n for x in v)
            return v
    return keys[-1][1]


class Gltf:
    def __init__(self, static=None):
        self.static = static
        self.doc = {
            "asset": {"version": "2.0", "generator": "3dsFM resources/banner/model.py"},
            "scene": 0,
            "scenes": [{"name": "Banner", "nodes": []}],
            "nodes": [], "meshes": [], "materials": [], "accessors": [],
            "bufferViews": [], "buffers": [], "animations": [],
        }
        self.blob = bytearray()
        self.channels = []
        self.samplers = []

    def view(self, data, target=None):
        while len(self.blob) % 4:
            self.blob.append(0)
        view = {"buffer": 0, "byteOffset": len(self.blob), "byteLength": len(data)}
        if target:
            view["target"] = target
        self.blob += data
        self.doc["bufferViews"].append(view)
        return len(self.doc["bufferViews"]) - 1

    def accessor(self, values, kind, component=5126, target=None, bounds=False):
        width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[kind]
        flat = [v for value in values for v in (value if width > 1 else (value,))]
        fmt = {5126: "f", 5123: "H"}[component]
        acc = {
            "bufferView": self.view(struct.pack("<%d%s" % (len(flat), fmt), *flat), target),
            "componentType": component, "count": len(values), "type": kind,
        }
        if bounds:
            acc["min"] = [min(v[i] for v in values) for i in range(width)] if width > 1 else [min(values)]
            acc["max"] = [max(v[i] for v in values) for i in range(width)] if width > 1 else [max(values)]
        self.doc["accessors"].append(acc)
        return len(self.doc["accessors"]) - 1

    def material(self, name, colour, roughness=0.35, texture=None):
        mat = {
            "name": name,
            "pbrMetallicRoughness": {
                "baseColorFactor": [*colour, 1.0], "metallicFactor": 0.0, "roughnessFactor": roughness,
            },
        }
        if texture is not None:
            mat["pbrMetallicRoughness"]["baseColorTexture"] = {"index": texture}
            # A cut-out, not a blend: blended materials make pycgfx reorder
            # bones and need a separate drawing pass.
            mat["alphaMode"] = "MASK"
            mat["alphaCutoff"] = 0.5
        self.doc["materials"].append(mat)
        return len(self.doc["materials"]) - 1

    def texture(self, png):
        self.doc.setdefault("images", []).append({"name": "logo", "bufferView": self.view(png), "mimeType": "image/png"})
        self.doc.setdefault("samplers", []).append({"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071})
        self.doc.setdefault("textures", []).append({"source": len(self.doc["images"]) - 1, "sampler": 0})
        return len(self.doc["textures"]) - 1

    def mesh(self, name, mesh, material):
        attributes = {
            "POSITION": self.accessor(mesh.positions, "VEC3", target=34962, bounds=True),
            "NORMAL": self.accessor(mesh.normals, "VEC3", target=34962),
        }
        if mesh.uvs:
            attributes["TEXCOORD_0"] = self.accessor(mesh.uvs, "VEC2", target=34962)
        assert len(mesh.positions) < 65536
        indices = self.accessor(mesh.indices, "SCALAR", component=5123, target=34963)
        self.doc["meshes"].append({"name": name, "primitives": [{"attributes": attributes, "indices": indices, "material": material}]})
        return len(self.doc["meshes"]) - 1

    def node(self, name, mesh=None, parent=None, translation=None, scale=None):
        node = {"name": name}
        if mesh is not None:
            node["mesh"] = mesh
        if translation:
            node["translation"] = list(translation)
        if scale:
            node["scale"] = list(scale)
        self.doc["nodes"].append(node)
        index = len(self.doc["nodes"]) - 1
        if parent is None:
            self.doc["scenes"][0]["nodes"].append(index)
        else:
            self.doc["nodes"][parent].setdefault("children", []).append(index)
        return index

    def animate(self, node, translation=None, rotation=None, scale=None):
        for path, keys, kind in (("translation", translation, "VEC3"), ("rotation", rotation, "VEC4"), ("scale", scale, "VEC3")):
            if not keys:
                continue
            if self.static is not None:
                self.doc["nodes"][node][path] = list(sample(keys, self.static))
                continue
            times = self.accessor([t for t, _ in keys], "SCALAR", bounds=True)
            values = self.accessor([v for _, v in keys], kind)
            self.samplers.append({"input": times, "output": values, "interpolation": "LINEAR"})
            self.channels.append({"sampler": len(self.samplers) - 1, "target": {"node": node, "path": path}})

    def write(self, path):
        if self.channels:
            self.doc["animations"].append({"name": "Banner", "channels": self.channels, "samplers": self.samplers})
        while len(self.blob) % 4:
            self.blob.append(0)
        self.doc["buffers"] = [{"byteLength": len(self.blob)}]
        text = json.dumps(self.doc, separators=(",", ":")).encode()
        text += b" " * (-len(text) % 4)
        with open(path, "wb") as out:
            out.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(text) + 8 + len(self.blob)))
            out.write(struct.pack("<I4s", len(text), b"JSON") + text)
            out.write(struct.pack("<I4s", len(self.blob), b"BIN\0") + self.blob)


def build(logo_png, out_path, static=None):
    g = Gltf(static)
    vinyl = g.material("Vinyl", (0.15, 0.17, 0.2), 0.25)
    groove_mat = g.material("Groove", (0.3, 0.33, 0.38), 0.35)
    label_mat = g.material("Label", (0.23, 0.59, 0.92), 0.3)
    ring_mat = g.material("LabelRing", (0.56, 0.82, 1.0), 0.35)
    spindle_mat = g.material("Spindle", (0.93, 0.95, 0.97), 0.4)
    arm_mat = g.material("Arm", (0.93, 0.95, 0.97), 0.3)
    blue = g.material("NoteBlue", (0.56, 0.82, 1.0), 0.3)
    yellow = g.material("NoteYellow", (1.0, 0.77, 0.1), 0.3)
    gold = g.material("Twinkle", (1.0, 0.85, 0.29), 0.4)
    logo = g.material("Logo", (1.0, 1.0, 1.0), 1.0, texture=g.texture(logo_png))

    root = g.node("Banner")

    # The record spins inside a rig that only sways, so the tilt is a tilt
    # and the spin is a spin.
    rig = g.node("RecordRig", parent=root, translation=(-9.6, -1.4, -1.0), scale=(1.25, 1.25, 1.25))
    g.animate(rig, *rig_tracks())
    rec = g.node("Record", parent=rig)
    g.node("RecordBody", g.mesh("RecordBody", record(), vinyl), parent=rec)
    g.node("RecordGroove", g.mesh("RecordGroove", groove(), groove_mat), parent=rec)
    g.node("RecordGrooveFill", g.mesh("RecordGrooveFill", groove_fill(), vinyl), parent=rec)
    g.node("RecordLabel", g.mesh("RecordLabel", label(), label_mat), parent=rec)
    g.node("RecordRing", g.mesh("RecordRing", label_ring(), ring_mat), parent=rec)
    g.node("RecordRingFill", g.mesh("RecordRingFill", label_fill(), label_mat), parent=rec)
    g.node("RecordSpindle", g.mesh("RecordSpindle", spindle(), spindle_mat), parent=rec)
    translation, rotation, scale = record_tracks()
    g.animate(rec, translation, rotation, scale)

    # The tonearm pivots at the record's upper right, in front of it.
    pivot = g.node("Arm", g.mesh("Arm", arm(), arm_mat), parent=rig, translation=(6.6, 5.4, 1.4))
    g.animate(pivot, *arm_track())

    # Notes burst out from under the needle (about (2.0, -2.8) in rig
    # units) and float off in three directions.
    for k, (material, start, end, size, delay, spin) in enumerate([
        (blue, (2.2, -1.8, 1.6), (5.4, 7.2, 2.2), 1.1, 0.0, 70),
        (yellow, (1.6, -2.2, 1.6), (-2.6, 8.6, 2.4), 0.95, 0.07, -80),
        (blue, (2.6, -2.6, 1.6), (8.8, 2.8, 2.0), 0.8, 0.14, 55),
    ]):
        node = g.node("Note%d" % (k + 1), g.mesh("Note%d" % (k + 1), note(), material), parent=rig)
        g.animate(node, *note_tracks(start, end, size, delay, spin))

    wordmark = g.node("Logo", g.mesh("Logo", plane(23.0, 11.5), logo), parent=root)
    g.animate(wordmark, *logo_tracks())

    sparkle = g.mesh("Twinkle", extrude(star(1.0, 0.46), 0.5, 0.1), gold)
    for k, (position, size, phase) in enumerate([((-0.6, 9.0, 1.0), 1.1, 0.0), ((18.0, -4.6, 2.0), 0.9, 2.1)]):
        node = g.node("Twinkle%d" % (k + 1), sparkle, parent=root)
        g.animate(node, *twinkle_tracks(position, size, phase))

    g.write(out_path)


if __name__ == "__main__":
    args = sys.argv[1:]
    static = None
    if "--static" in args:
        i = args.index("--static")
        static = float(args[i + 1])
        del args[i:i + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    with open(args[0], "rb") as source:
        build(source.read(), args[1], static)
