"""SM_Canister, SM_Tub - the two big hand-held vessels (after SM_Bowl and SM_Bucket).

Both stand on the ground: origin = centre of the base, Z up. Both pour towards +X
(the rim is pinched into a spout there), the game shows an asphalt level disc in
the round cavity, so the INSIDE MEASUREMENTS below are part of the contract.

SM_Canister (0.29 x 0.23 x 0.40 m): tall steel pail, 20 l, open top.
  Tapered body: outer radius 0.105 at Z = 0 -> 0.115 at the rim (Z = 0.40).
  Inside: floor at Z = 0.015 (radius 0.090), rim inner radius 0.105 at Z = 0.40.
  Pouring beak on +X: the rim is pulled out to X = 0.145 (0.03 beyond the rim).
  Grip loop on -X: a flat strap in the X-Z plane, Z 0.30 .. 0.38, out to X = -0.145.
  Two dark hoops (Z = 0.10, 0.19), a DIA_Yellow band Z 0.25 .. 0.34, dark rim.
  Envelope: X +-0.145, Y +-0.115, Z [0, 0.40].

SM_Tub (0.51 x 0.58 x 0.22 m): wide shallow mortar tub, 36 l.
  Outer radius 0.170 at Z = 0 -> 0.245 at the rim (Z = 0.22).
  Inside: floor at Z = 0.015 (radius 0.150), rim inner radius 0.235 at Z = 0.22.
  Two chunky D-handles at the rim on +Y and -Y (grip bar at Y = +-0.275,
  Z = 0.20, 0.14 long), pouring lip on +X pulled out to X = 0.265.
  DIA_Orange body, DIA_DarkSteel rim edge and handles.
  Envelope: X [-0.245, 0.265], Y +-0.29, Z [0, 0.22].

Run:  blender -b -P Tools/ModelGen/vessels.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

METAL, DARK, BAND, PLASTIC = "DIA_Steel", "DIA_DarkSteel", "DIA_Yellow", "DIA_Orange"
SEGMENTS = 16


def paint(obj, mat, z_lo, z_hi):
    """Give the faces of `obj` that lie completely within z_lo .. z_hi the palette
    material `mat` (join() keeps per-face materials of a part)."""
    me = obj.data
    me.materials.append(c.material(mat))
    slot = len(me.materials) - 1
    for poly in me.polygons:
        zs = [me.vertices[i].co.z for i in poly.vertices]
        if min(zs) >= z_lo - 1e-6 and max(zs) <= z_hi + 1e-6:
            poly.material_index = slot
    return obj


def pinch(obj, src, dst):
    """Move the vertex of `obj` at `src` to `dst`: pulls the rim out into a spout."""
    hit = 0
    for v in obj.data.vertices:
        if all(abs(v.co[i] - src[i]) < 1e-6 for i in range(3)):
            v.co = dst
            hit += 1
    assert hit == 1, (src, hit)
    return obj


def build_canister():
    c.reset_scene()
    r0, r1, h = 0.105, 0.115, 0.40

    def radius(z):
        return r0 + (r1 - r0) * z / h

    body = c.lathe([(0.0, 0.0), (r0, 0.0), (radius(0.25), 0.25), (radius(0.34), 0.34),
                    (radius(0.385), 0.385), (r1, h), (0.105, h), (0.090, 0.015), (0.0, 0.015)],
                   (0, 0, 0), "Z", METAL, segments=SEGMENTS)
    paint(body, BAND, 0.25, 0.34)
    paint(body, DARK, 0.385, h)                       # rolled rim: outer edge and top
    pinch(body, (r1, 0, h), (r1 + 0.03, 0, h))        # beak
    pinch(body, (0.105, 0, h), (0.132, 0, 0.392))
    parts = [body]
    for z in (0.10, 0.19):                            # reinforcing hoops
        parts.append(c.ring(radius(z) - 0.003, radius(z) + 0.004, z - 0.008, z + 0.008,
                            (0, 0, 0), "Z", DARK, SEGMENTS))
    # Grip loop on -X: two lugs and a flat strap.
    parts.append(c.box((0.040, 0.036, 0.016), (-0.125, 0, 0.372), DARK))
    parts.append(c.box((0.040, 0.036, 0.016), (-0.125, 0, 0.308), DARK))
    parts.append(c.box((0.014, 0.044, 0.080), (-0.138, 0, 0.340), DARK))
    c.finish(parts, "SM_Canister", silhouette=False)


def build_tub():
    c.reset_scene()
    r0, r1, h = 0.170, 0.245, 0.22

    def radius(z):
        return r0 + (r1 - r0) * z / h

    body = c.lathe([(0.0, 0.0), (r0, 0.0), (radius(0.195), 0.195), (r1, h), (0.235, h),
                    (0.150, 0.015), (0.0, 0.015)], (0, 0, 0), "Z", PLASTIC, segments=SEGMENTS)
    paint(body, DARK, 0.195, h)                       # rim edge
    pinch(body, (r1, 0, h), (r1 + 0.02, 0, h))        # pouring lip
    pinch(body, (0.235, 0, h), (0.254, 0, 0.212))
    parts = [body]
    for side in (1, -1):                              # D-handles
        for x in (-0.0575, 0.0575):
            parts.append(c.box((0.030, 0.060, 0.030), (x, side * 0.260, 0.200), DARK))
        parts.append(c.box((0.145, 0.030, 0.030), (0, side * 0.275, 0.200), DARK))
    c.finish(parts, "SM_Tub", silhouette=False)


build_canister()
build_tub()
