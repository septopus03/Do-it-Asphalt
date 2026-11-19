"""SM_Fence, SM_Barrel, SM_Lamp, SM_DirtPile - site dressing.

SM_Fence (0.08 x 2.0 x 1.8 m; origin = centre of the footprint, on the ground):
  one panel of mobile construction fence: dark tube frame, an infill of ten
  vertical steel strips, an orange band across the middle and a concrete foot
  under each post. Panels tile along Y at a 2.0 m pitch; the feet reach the
  panel ends, so the feet of neighbouring panels meet.
  Envelope: X +-0.04, Y +-1.0, Z [0, 1.8]. 4 materials.

SM_Barrel (0.6 x 0.6 x 0.9 m; origin = centre of the footprint, on the ground):
  orange steel drum with two dark rolled hoops, a dark rim around the recessed
  lid and a bung on it. Envelope: X +-0.3, Y +-0.3, Z [0, 0.9]. 2 materials.

SM_Lamp (0.4 x 1.2 x 5.0 m; origin = the foot of the pole, on the ground; the
arm points towards +Y):
  concrete block, tapered dark pole, an arm with a brace and at its end a
  yellow floodlight with its outer end raised, so the white lens underneath
  faces down and outwards.
  Envelope: X +-0.2, Y [-0.2, 1.0], Z [0, 5.0]. 4 materials.

SM_DirtPile (2.0 x 2.0 x 1.0 m; origin = centre of the footprint, on the ground):
  irregular mound of DIA_Dirt with a smaller mound leaning on it and a few
  stones at the foot. The shape comes from a fixed random seed, so every run
  gives the same pile. Envelope: X +-1.0, Y +-1.0, Z [0, 1.0]. 2 materials.

Run:  blender -b -P Tools/ModelGen/site_props.py
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

FRAME = "DIA_DarkSteel"

# --- fence ------------------------------------------------------------------
FENCE_HALF_Y = 1.0
FENCE_TOP = 1.8
FENCE_FOOT = (0.08, 0.36, 0.12)      # X x Y x Z; its X size is the envelope depth
FENCE_TUBE = 0.04                    # square frame tube
FENCE_RAIL_Z = 0.22                  # underside of the bottom rail
FENCE_STRIPS = 10
FENCE_STRIP = (0.012, 0.12)          # thickness (X), width (Y)
FENCE_BAND = (0.024, 0.95, 1.07)     # thickness (X), bottom, top

# --- barrel -----------------------------------------------------------------
BARREL_R = 0.28                      # body
BARREL_HOOP_R = 0.30                 # hoops and top rim: the envelope radius
BARREL_TOP = 0.90
BARREL_RIM = 0.05                    # height of the flared rim at the top
BARREL_LID = (0.265, 0.875)          # radius and height of the recessed lid
BARREL_BUNG = (0.04, 0.02, 0.16)     # radius, height, distance from the axis
BARREL_HOOP_Z = (0.31, 0.59)         # centres of the two rolled hoops
BARREL_HOOP_HALF = 0.03
BARREL_SEGMENTS = 12

# --- lamp -------------------------------------------------------------------
LAMP_BLOCK = (0.40, 0.40, 0.35)      # concrete block; its X size is the envelope width
LAMP_TOP = 5.0
LAMP_REACH = 1.0                     # +Y end of the floodlight
LAMP_HEAD = (0.40, 0.30, 0.13)       # floodlight housing before it is tilted
LAMP_LENS = (0.34, 0.24, 0.02)
LAMP_TILT = 25.0                     # degrees the outer end of the head is raised
LAMP_SOCKET = 0.08                   # the arm enters the housing this far behind its centre
POLE_R = (0.07, 0.045)               # bottom, top
POLE_TOP = 4.80
ARM_R = 0.035

# --- dirt pile --------------------------------------------------------------
PILE_SEED = 7
PILE_HALF = 1.0
PILE_HEIGHT = 1.0


def build_fence():
    name = "SM_Fence"
    c.reset_scene()
    parts = []

    post_y = FENCE_HALF_Y - FENCE_TUBE / 2
    foot_y = FENCE_HALF_Y - FENCE_FOOT[1] / 2
    for side in (1, -1):
        parts.append(c.box(FENCE_FOOT, (0, side * foot_y, FENCE_FOOT[2] / 2), "DIA_Concrete"))
        parts.append(c.box((FENCE_TUBE, FENCE_TUBE, FENCE_TOP - FENCE_FOOT[2]),
                           (0, side * post_y, (FENCE_TOP + FENCE_FOOT[2]) / 2), FRAME))
    inner = FENCE_HALF_Y - FENCE_TUBE                # rails and infill fit between the posts
    rail_top = FENCE_TOP - FENCE_TUBE
    for z in (FENCE_RAIL_Z, rail_top):
        parts.append(c.box((FENCE_TUBE, 2 * inner, FENCE_TUBE), (0, 0, z + FENCE_TUBE / 2), FRAME))

    # Infill: vertical strips between the rails (their ends are hidden in the rails).
    thick, width = FENCE_STRIP
    pitch = 2 * inner / FENCE_STRIPS
    z0, z1 = FENCE_RAIL_Z + FENCE_TUBE, rail_top
    for i in range(FENCE_STRIPS):
        y = -inner + (i + 0.5) * pitch
        parts.append(c.extrude([(-thick / 2, y - width / 2), (thick / 2, y - width / 2),
                                (thick / 2, y + width / 2), (-thick / 2, y + width / 2)],
                               z0, z1, "Z", "DIA_Steel", cap_lo=False, cap_hi=False))
    band_thick, band_z0, band_z1 = FENCE_BAND
    parts.append(c.box((band_thick, 2 * inner, band_z1 - band_z0), (0, 0, (band_z0 + band_z1) / 2),
                       "DIA_Orange"))

    return c.finish(parts, name)


def build_barrel():
    name = "SM_Barrel"
    c.reset_scene()
    parts = []

    def band(profile, mat):
        return c.lathe(profile, (0, 0, 0), "Z", mat, BARREL_SEGMENTS, cap_start=False, cap_end=False)

    # One continuous wall from the ground up, cut into bands by colour.
    r, hoop = BARREL_R, BARREL_HOOP_R
    z = 0.0
    for centre in BARREL_HOOP_Z:
        parts.append(band([(r, z), (r, centre - BARREL_HOOP_HALF)], "DIA_Orange"))
        parts.append(band([(r, centre - BARREL_HOOP_HALF), (hoop, centre),
                           (r, centre + BARREL_HOOP_HALF)], FRAME))
        z = centre + BARREL_HOOP_HALF
    parts.append(band([(r, z), (r, BARREL_TOP - BARREL_RIM)], "DIA_Orange"))
    # top rim: flares out to the envelope radius, then falls inwards to the lid
    lid_r, lid_z = BARREL_LID
    parts.append(c.lathe([(r, BARREL_TOP - BARREL_RIM), (hoop, BARREL_TOP), (lid_r, lid_z)],
                         (0, 0, 0), "Z", FRAME, BARREL_SEGMENTS, cap_start=False))
    bung_r, bung_h, bung_x = BARREL_BUNG
    parts.append(c.cylinder(bung_r, bung_h, (bung_x, 0, lid_z + bung_h / 2), "Z", "DIA_Orange",
                            segments=6, cap_bottom=False))

    return c.finish(parts, name)


def build_lamp():
    name = "SM_Lamp"
    c.reset_scene()
    parts = []

    parts.append(c.box(LAMP_BLOCK, (0, 0, LAMP_BLOCK[2] / 2), "DIA_Concrete", bevel=0.03))
    parts.append(c.cone(POLE_R[0], POLE_R[1], POLE_TOP - LAMP_BLOCK[2],
                        (0, 0, (POLE_TOP + LAMP_BLOCK[2]) / 2), "Z", FRAME, segments=8,
                        cap_bottom=False))
    parts.append(c.box((0.14, 0.10, 0.30), (0, -POLE_R[0] - 0.02, 1.30), "DIA_Yellow"))   # switch box

    # Floodlight: built flat around the origin (lens underneath), tilted up, then moved
    # so that it ends exactly at the top and at the +Y end of the envelope.
    housing = c.box(LAMP_HEAD, (0, 0, 0), "DIA_Yellow")
    lens = c.box(LAMP_LENS, (0, 0, -(LAMP_HEAD[2] + LAMP_LENS[2]) / 2), "DIA_White")
    head = [housing, lens]
    for part in head:
        c.rotate(part, "X", LAMP_TILT)
    hi = c.bounds(housing)[1]
    shift = (0.0, LAMP_REACH - max(hi[1], c.bounds(lens)[1][1]), LAMP_TOP - hi[2])
    for part in head:
        c.translate(part, shift)
    parts += head

    # Arm from the pole top into the housing (behind its centre), and a brace under it.
    lo, hi = c.bounds(housing)
    tilt = math.radians(LAMP_TILT)
    socket = (0.0, (lo[1] + hi[1]) / 2 - LAMP_SOCKET * math.cos(tilt),
              (lo[2] + hi[2]) / 2 - LAMP_SOCKET * math.sin(tilt))
    arm_root = (0.0, 0.0, POLE_TOP - 0.05)
    parts.append(c.beam(arm_root, socket, ARM_R, FRAME, segments=6, cap_start=False, cap_end=False))
    half_way = tuple((a + b) / 2 for a, b in zip(arm_root, socket))
    parts.append(c.beam((0.0, 0.0, POLE_TOP - 0.55), half_way, 0.025, FRAME, segments=6,
                        cap_start=False, cap_end=False))

    return c.finish(parts, name)


def mound(rng, centre, radius, height, segments, levels, mat="DIA_Dirt"):
    """Irregular heap: `levels` rings of `segments` points shrinking towards an
    off-centre top, every point pushed in or out a little; triangles only, with
    alternating diagonals. Open underneath (it stands on the ground)."""
    cx, cy = centre
    top = (cx + rng.uniform(-0.15, 0.15) * radius, cy + rng.uniform(-0.15, 0.15) * radius, height)
    verts, rings = [], []
    for level in range(levels):
        t = level / levels                           # 0 at the ground, 1 at the top
        ring = []
        for k in range(segments):
            a = 2 * math.pi * (k + 0.5 * (level % 2)) / segments
            r = radius * (1.0 - t) ** 0.8 * rng.uniform(0.84, 1.0)
            z = 0.0 if level == 0 else height * (t ** 0.85) * rng.uniform(0.88, 1.08)
            # lean every ring a little towards the top point
            ring.append(len(verts))
            verts.append((cx + (top[0] - cx) * t + r * math.cos(a),
                          cy + (top[1] - cy) * t + r * math.sin(a), z))
        rings.append(ring)
    apex = len(verts)
    verts.append(top)
    faces = []
    for lo, hi in zip(rings, rings[1:]):
        for k in range(segments):
            k1 = (k + 1) % segments
            if k % 2:
                faces += [(lo[k], lo[k1], hi[k1]), (lo[k], hi[k1], hi[k])]
            else:
                faces += [(lo[k], lo[k1], hi[k]), (lo[k1], hi[k1], hi[k])]
    last = rings[-1]
    faces += [(last[k], last[(k + 1) % segments], apex) for k in range(segments)]
    return c.mesh(verts, faces, mat)


def fit(parts, lo, hi):
    """Stretch and move `parts` together so their common envelope becomes lo .. hi."""
    boxes = [c.bounds(p) for p in parts]
    mn = [min(b[0][i] for b in boxes) for i in range(3)]
    mx = [max(b[1][i] for b in boxes) for i in range(3)]
    factors = [(hi[i] - lo[i]) / (mx[i] - mn[i]) for i in range(3)]
    for p in parts:
        c.translate(p, [-mn[i] for i in range(3)])
        c.scale(p, factors)
        c.translate(p, lo)
    return parts


def build_dirt_pile():
    name = "SM_DirtPile"
    c.reset_scene()
    rng = random.Random(PILE_SEED)
    parts = [
        mound(rng, (-0.10, 0.05), 0.95, 1.0, segments=14, levels=4),
        mound(rng, (0.50, -0.45), 0.50, 0.50, segments=9, levels=3),
    ]
    for x, y, size, turn in ((0.65, 0.55, 0.20, 25.0), (-0.55, -0.72, 0.15, 60.0), (0.82, 0.30, 0.11, 10.0)):
        height = size * 0.7
        stone = c.box((size, size * 0.8, height), (x, y, height / 2), "DIA_Concrete", bevel=size * 0.2)
        parts.append(c.rotate(stone, "Z", turn, pivot=(x, y, 0)))
    fit(parts, (-PILE_HALF, -PILE_HALF, 0.0), (PILE_HALF, PILE_HALF, PILE_HEIGHT))

    return c.finish(parts, name)


if __name__ == "__main__":
    build_fence()
    build_barrel()
    build_lamp()
    build_dirt_pile()
