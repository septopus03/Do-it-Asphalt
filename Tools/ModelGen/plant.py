"""SM_Plant, SM_Plant_AddonA, SM_Plant_AddonB - the asphalt plant and its two add-ons.

SM_Plant (6.8 x 4.0 x 5.0 m; origin = centre of the body's footprint, on the ground;
the body takes X -3.0 .. +3.0, the discharge chute reaches X = +3.4):
  concrete slab; along the +Y side the production line, read from -X to +X:
  feed hood (fed by the conveyor that climbs diagonally over the drum from the
  aggregate hopper on the -Y side) -> tilted mixing drum with two orange tyre rings -> discharge
  box -> bucket elevator -> storage silo (top at Z = 5.0) on four legs at Y = 0.
  In the -X/-Y corner the dust filter with the chimney, in the +X/-Y corner the
  operator's booth with windows. The DISCHARGE CHUTE leaves the gate under the
  silo towards +X; its mouth is the vertical opening at X = 3.4 centred on
  CHUTE_END = (3.4, 0, 1.8): dark DIA_Asphalt inside a thick DIA_Orange rim.

SM_Plant_AddonA (1.6 x 1.6 x 5.5 m): second, taller silo on legs with a ladder
on the +X side. SM_Plant_AddonB (2.4 x 1.8 x 3.0 m): horizontal storage
tank on two leg frames, with a level gauge on the +X end and a drain valve.
Both: origin = centre of the footprint, on the ground, standing on their own pad.

The palette allows 5 materials per model, so the three share one scheme:
DIA_Yellow vessels, DIA_Concrete slab / ducts / cladding, DIA_Asphalt for every
dark part (frames, legs, belt, the chute mouth), DIA_Orange accents, DIA_Glass.

Run:  blender -b -P Tools/ModelGen/plant.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

BODY = "DIA_Yellow"
GREY = "DIA_Concrete"
DARK = "DIA_Asphalt"
ACCENT = "DIA_Orange"
GLASS = "DIA_Glass"

SLAB = 0.25                      # top of the plant's concrete slab
CHUTE_END = (3.4, 0.0, 1.8)      # centre of the chute mouth (docs/03 chapter 4.2)
SILO_XY = (1.9, 0.0)
DRUM_C = (-0.75, 0.9, 1.6)       # centre of the mixing drum
DRUM_TILT = 9.0                  # degrees, the +X (discharge) end is the low one


def silo(x, y, r, z_floor, z_outlet, z_cyl0, z_cyl1, z_roof, segments=16, open_side=None):
    """Silo on four legs: outlet cone, cylinder z_cyl0..z_cyl1, conical roof up to
    z_roof, ring beam, diagonal braces (none on `open_side`, 0 = +X, 1 = +Y, ...)."""
    parts = []
    k = r * 0.68
    corners = [(x + k, y + k), (x - k, y + k), (x - k, y - k), (x + k, y - k)]
    for cx, cy in corners:
        parts.append(c.box((0.12, 0.12, z_cyl0 + 0.04 - z_floor),
                           (cx, cy, (z_cyl0 + 0.04 + z_floor) / 2), DARK))
        parts.append(c.box((0.24, 0.24, 0.05), (cx, cy, z_floor + 0.025), DARK))
    # sides: +X between corner 3 and 0, +Y 0-1, -X 1-2, -Y 2-3
    for side, (i, j) in enumerate(((3, 0), (0, 1), (1, 2), (2, 3))):
        if side == open_side:
            continue
        a, b = corners[i], corners[j]
        z_lo, z_hi = z_floor + 0.25, z_outlet - 0.15
        parts.append(c.bar((a[0], a[1], z_lo), (b[0], b[1], z_hi), 0.05, 0.07, DARK))
        parts.append(c.bar((b[0], b[1], z_lo), (a[0], a[1], z_hi), 0.05, 0.07, DARK))
    parts.append(c.ring(r - 0.05, r + 0.06, z_cyl0 - 0.12, z_cyl0 + 0.04, (x, y, 0), "Z", DARK, segments))
    r_out = 0.22 * r / 0.8
    parts.append(c.lathe([(r_out, z_outlet), (r, z_cyl0), (r, z_cyl1), (0.3 * r, z_roof)],
                         (x, y, 0), "Z", BODY, segments))
    band = z_cyl0 + 0.62 * (z_cyl1 - z_cyl0)
    parts.append(c.ring(r - 0.02, r + 0.025, band, band + 0.16, (x, y, 0), "Z", ACCENT, segments))
    return parts


def railing(x, y, r, z_foot, z_top, posts=8, segments=16):
    """Orange guard rail around a silo roof: posts and a top hoop ending at z_top."""
    parts = [c.ring(r - 0.025, r + 0.025, z_top - 0.05, z_top, (x, y, 0), "Z", ACCENT, segments)]
    for i in range(posts):
        a = 2 * math.pi * (i + 0.5) / posts
        px, py = x + r * math.cos(a), y + r * math.sin(a)
        parts.append(c.box((0.035, 0.035, z_top - 0.04 - z_foot), (px, py, (z_top - 0.04 + z_foot) / 2),
                           ACCENT))
    return parts


def build_plant():
    name = "SM_Plant"
    c.reset_scene()
    parts = []

    # ------------------------------------------------------------ concrete slab
    parts.append(c.box((6.0, 4.0, SLAB), (0, 0, SLAB / 2), GREY, bevel=0.04))

    # ------------------------------------------------------------ mixing drum
    dx, dy, dz = DRUM_C
    half, r = 1.2, 0.55
    drum = [c.lathe([(0.40, -half), (r, -half + 0.12), (r, half - 0.12), (0.40, half)],
                    (0, 0, 0), "X", BODY, segments=16)]
    for h in (-0.62, 0.62):
        drum.append(c.ring(r - 0.02, r + 0.09, h - 0.07, h + 0.07, (0, 0, 0), "X", ACCENT, 16))
    drum.append(c.ring(r - 0.02, r + 0.05, -0.05, 0.05, (0, 0, 0), "X", DARK, 16))   # drive gear
    for p in drum:
        c.rotate(p, "Y", DRUM_TILT)
        c.translate(p, DRUM_C)
    parts += drum
    slope = math.tan(math.radians(DRUM_TILT))
    # skid frame and two saddles carrying the tyre rings
    parts.append(c.box((2.3, 0.16, 0.14), (dx, dy - 0.45, SLAB + 0.07), DARK))
    parts.append(c.box((2.3, 0.16, 0.14), (dx, dy + 0.45, SLAB + 0.07), DARK))
    for h in (-0.62, 0.62):
        zc = dz - h * slope
        top = zc - 0.50
        parts.append(c.extrude([(dy - 0.56, SLAB), (dy + 0.56, SLAB), (dy + 0.42, top), (dy - 0.42, top)],
                               dx + h - 0.11, dx + h + 0.11, "X", DARK))
    # drive motor beside the gear ring
    parts.append(c.box((0.34, 0.26, 0.30), (dx, dy + 0.72, SLAB + 0.29), ACCENT))

    # ------------------------------------------------------------ feed end (-X): hood, conveyor, hopper
    hood_x = (-2.45, -1.85)
    hx = (hood_x[0] + hood_x[1]) / 2
    parts.append(c.box((0.6, 0.9, 2.45 - SLAB), (hx, dy, (2.45 + SLAB) / 2), GREY, bevel=0.03))
    funnel = c.cone(0.30, 0.52, 0.30, (hx, dy, 2.60), "Z", GREY, segments=4, cap_bottom=False)
    parts.append(c.rotate(funnel, "Z", 45, (hx, dy, 0)))
    # burner on the outer face of the hood
    parts.append(c.cylinder(0.17, 0.30, (hx, dy + 0.58, 1.75), "Y", ACCENT, segments=8, cap_bottom=False))
    parts.append(c.box((0.30, 0.24, 0.30), (hx, dy + 0.75, 1.42), DARK))

    # conveyor: climbs diagonally from under the hopper (-Y side) over the drum to the funnel
    hop = (-0.30, -1.45)
    belt0 = (hop[0] + 0.12, hop[1] - 0.14, 0.80)
    belt1 = (hx + 0.22, dy - 0.20, 2.92)
    d = [belt1[i] - belt0[i] for i in range(3)]
    plan = math.hypot(d[0], d[1])
    length = math.hypot(plan, d[2])
    side = (-d[1] / plan, d[0] / plan)              # horizontal, across the belt
    up = (-d[0] / plan * d[2] / length, -d[1] / plan * d[2] / length, plan / length)
    parts.append(c.bar(belt0, belt1, 0.50, 0.12, BODY))
    lift = 0.075                                    # the belt lies on top of the frame
    parts.append(c.bar([belt0[i] + up[i] * lift for i in range(3)],
                       [belt1[i] + up[i] * lift for i in range(3)], 0.38, 0.05, DARK))
    for p in (belt0, belt1):
        parts.append(c.beam((p[0] - side[0] * 0.27, p[1] - side[1] * 0.27, p[2]),
                            (p[0] + side[0] * 0.27, p[1] + side[1] * 0.27, p[2]), 0.11, DARK, segments=8))
    for t in (0.36, 0.66):                          # trestles
        m = [belt0[i] + d[i] * t for i in range(3)]
        feet = []
        for sgn in (-1, 1):
            foot = (m[0] + sgn * side[0] * 0.45, m[1] + sgn * side[1] * 0.45, SLAB)
            feet.append(foot)
            parts.append(c.bar(foot, (m[0] + sgn * side[0] * 0.2, m[1] + sgn * side[1] * 0.2, m[2] - 0.03),
                               0.07, 0.07, DARK))
        zt = SLAB + (m[2] - SLAB) * 0.45
        w = 0.45 - 0.25 * 0.45
        parts.append(c.bar((m[0] - side[0] * w, m[1] - side[1] * w, zt),
                           (m[0] + side[0] * w, m[1] + side[1] * w, zt), 0.06, 0.06, DARK))
    # aggregate hopper over the tail of the conveyor
    hopper = c.cone(0.26, 0.70, 0.80, (hop[0], hop[1], 1.75), "Z", BODY, segments=4, cap_top=False)
    parts.append(c.rotate(hopper, "Z", 45, (hop[0], hop[1], 0)))
    rim = c.cone(0.72, 0.72, 0.12, (hop[0], hop[1], 2.21), "Z", ACCENT, segments=4,
                 cap_top=False, cap_bottom=False)
    parts.append(c.rotate(rim, "Z", 45, (hop[0], hop[1], 0)))
    heap = c.cone(0.70, 0.12, 0.22, (hop[0], hop[1], 2.26), "Z", DARK, segments=4, cap_bottom=False)
    parts.append(c.rotate(heap, "Z", 45, (hop[0], hop[1], 0)))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(c.bar((hop[0] + sx * 0.47, hop[1] + sy * 0.47, SLAB),
                               (hop[0] + sx * 0.40, hop[1] + sy * 0.40, 2.05), 0.08, 0.08, DARK))

    # ------------------------------------------------------------ discharge end (+X): box, elevator
    parts.append(c.box((0.62, 0.86, 2.0 - SLAB), (0.72, dy, (2.0 + SLAB) / 2), GREY, bevel=0.03))
    parts.append(c.box((0.36, 0.36, 2.60), (0.72, dy, 3.28), BODY))
    parts.append(c.box((0.50, 0.50, 0.42), (0.72, dy, 4.76), ACCENT, bevel=0.03))
    parts.append(c.bar((0.90, dy - 0.15, 4.72), (SILO_XY[0] - 0.33, 0.30, 4.60), 0.24, 0.24, GREY))

    # ------------------------------------------------------------ dust filter and chimney (-X / -Y corner)
    fx, fy = -2.15, -1.40
    parts.append(c.box((1.4, 1.0, 1.85), (fx, fy, SLAB + 0.925), GREY, bevel=0.04))
    for x in (-0.45, 0.0, 0.45):                    # ribs on the face towards the drum
        parts.append(c.box((0.08, 0.05, 1.5), (fx + x, fy + 0.515, SLAB + 0.95), DARK))
    parts.append(c.cylinder(0.17, 1.40, (hx, (fy + 0.5 + dy - 0.45) / 2, 1.70), "Y", GREY, segments=8,
                            cap_bottom=False, cap_top=False))                # exhaust duct from the hood
    parts.append(c.lathe([(0.42, 2.08), (0.25, 2.45), (0.25, 4.42)], (fx, fy, 0), "Z", GREY,
                         segments=12, cap_start=False, cap_end=False))
    parts.append(c.cylinder(0.29, 0.30, (fx, fy, 4.55), "Z", DARK, segments=12))
    parts.append(c.ring(0.24, 0.285, 3.55, 3.75, (fx, fy, 0), "Z", ACCENT, 12))

    # ------------------------------------------------------------ silo, gate, DISCHARGE CHUTE
    sx, sy = SILO_XY
    parts += silo(sx, sy, 0.8, SLAB, 2.30, 2.85, 4.50, 4.75, open_side=0)
    parts.append(c.cylinder(0.15, 0.27, (sx, sy, 4.865), "Z", GREY, segments=8, cap_bottom=False))
    parts += railing(sx, sy, 0.72, 4.50, 5.0)
    parts.append(c.box((0.52, 0.52, 0.26), (sx, sy, 2.26), DARK))          # slide gate

    ex, ey, ez = CHUTE_END
    rim_x0 = ex - 0.13
    slope_c = (2.24 - ez) / (ex - sx)               # centre line: gate -> CHUTE_END
    zc = lambda x: ez + (ex - x) * slope_c          # noqa: E731
    hh, hw = 0.17, 0.22
    duct_x1 = rim_x0 + 0.02
    parts.append(c.extrude([(zc(sx) - hh, sx), (zc(duct_x1) - hh, duct_x1),
                            (zc(duct_x1) + hh, duct_x1), (zc(sx) + hh, sx)],
                           ey - hw, ey + hw, "Y", GREY))
    # mouth: dark opening recessed 3 cm inside a thick orange frame, both vertical at X = ex
    mw, mh, t = 0.25, 0.21, 0.10                    # half width, half height of the opening; rim
    parts.append(c.box((0.12, 2 * mw + 0.02, 2 * mh + 0.02), (ex - 0.03 - 0.06, ey, ez), DARK))
    mouth = [
        c.box((0.13, 2 * (mw + t), t), (ex - 0.065, ey, ez + mh + t / 2), ACCENT),
        c.box((0.13, 2 * (mw + t), t), (ex - 0.065, ey, ez - mh - t / 2), ACCENT),
        c.box((0.13, t, 2 * mh), (ex - 0.065, ey + mw + t / 2, ez), ACCENT),
        c.box((0.13, t, 2 * mh), (ex - 0.065, ey - mw - t / 2, ez), ACCENT),
    ]
    parts += mouth
    # hangers from the front legs
    k = 0.8 * 0.68
    for side in (-1, 1):
        parts.append(c.bar((sx + k, sy + side * k, 2.75), (2.95, ey + side * hw, zc(2.95) + hh - 0.02),
                           0.04, 0.04, DARK))
    # self-check of the functional point
    lo = [min(c.bounds(p)[0][i] for p in mouth) for i in range(3)]
    hi = [max(c.bounds(p)[1][i] for p in mouth) for i in range(3)]
    centre = [hi[0], (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2]
    assert all(abs(centre[i] - CHUTE_END[i]) < 1e-6 for i in range(3)), centre

    # ------------------------------------------------------------ operator's booth (+X / -Y corner)
    bx, by_ = (1.75, 2.85), (-1.9, -0.85)
    bcx, bcy = sum(bx) / 2, sum(by_) / 2
    parts.append(c.box((bx[1] - bx[0], by_[1] - by_[0], 2.05), (bcx, bcy, SLAB + 1.025), GREY))
    parts.append(c.box((bx[1] - bx[0] + 0.16, by_[1] - by_[0] + 0.16, 0.10), (bcx, bcy, SLAB + 2.10), DARK))
    parts.append(c.box((0.03, 0.75, 0.70), (bx[1] + 0.005, bcy, SLAB + 1.45), GLASS))       # window +X
    parts.append(c.box((0.50, 0.03, 0.70), (bx[1] - 0.38, by_[1] + 0.005, SLAB + 1.45), GLASS))  # window +Y
    parts.append(c.box((0.42, 0.03, 1.75), (bx[0] + 0.30, by_[1] + 0.005, SLAB + 0.875), DARK))  # door +Y
    parts.append(c.box((0.05, 0.85, 0.05), (bx[1] + 0.015, bcy, SLAB + 1.07), DARK))        # sill

    c.assert_outward(parts)
    obj = c.join(parts, name)
    c.export(obj, name)
    c.render_previews(obj, name)
    return obj


def build_addon_a():
    """Second silo with a ladder, 1.6 x 1.6 x 5.5 m."""
    name = "SM_Plant_AddonA"
    c.reset_scene()
    pad = 0.15
    parts = [c.box((1.6, 1.6, pad), (0, 0, pad / 2), GREY, bevel=0.03)]
    r = 0.68
    parts += silo(0, 0, r, pad, 1.45, 2.0, 5.0, 5.25)
    parts.append(c.cylinder(0.13, 0.28, (0, 0, 5.36), "Z", GREY, segments=8, cap_bottom=False))
    parts += railing(0, 0, 0.60, 5.0, 5.5)
    parts.append(c.box((0.40, 0.40, 0.22), (0, 0, 1.40), DARK))             # gate under the cone
    parts.append(c.cylinder(0.09, 0.30, (0, 0, 1.15), "-Z", ACCENT, segments=8, cap_bottom=False))

    # ladder on the +X side, from the pad to the roof
    lx, half = 0.765, 0.19
    z0, z1 = pad, 5.42
    for side in (-1, 1):
        parts.append(c.box((0.045, 0.045, z1 - z0), (lx, side * half, (z0 + z1) / 2), DARK))
    z = z0 + 0.30
    while z < z1 - 0.1:
        parts.append(c.box((0.03, 2 * half, 0.03), (lx, 0, z), DARK))
        z += 0.32
    for z in (2.3, 3.6, 4.9):                        # stand-offs to the shell
        for side in (-1, 1):
            parts.append(c.box((lx - r + 0.06, 0.035, 0.035), ((lx + r - 0.06) / 2 + 0.02, side * half, z), DARK))
    parts.append(c.box((0.07, 2 * half + 0.09, 0.07), (lx, 0, 2.62), ACCENT))   # ladder guard bar

    c.assert_outward(parts)
    obj = c.join(parts, name)
    c.export(obj, name)
    c.render_previews(obj, name)
    return obj


def build_addon_b():
    """Horizontal storage tank on legs, 2.4 x 1.8 x 3.0 m."""
    name = "SM_Plant_AddonB"
    c.reset_scene()
    pad = 0.15
    parts = [c.box((2.4, 1.8, pad), (0, 0, pad / 2), GREY, bevel=0.03)]
    r, zc, half = 0.72, 2.04, 1.12
    parts.append(c.lathe([(0.0, -half), (0.46, -half + 0.07), (r, -half + 0.27), (r, half - 0.27),
                          (0.46, half - 0.07), (0.0, half)], (0, 0, zc), "X", BODY, segments=16))
    for x in (-0.58, 0.58):
        parts.append(c.ring(r - 0.02, r + 0.03, x - 0.07, x + 0.07, (0, 0, zc), "X", ACCENT, 16))
        # leg frame: two splayed legs, a saddle and a cross tie
        top = zc - r + 0.20
        parts.append(c.extrude([(-0.56, top - 0.30), (0.56, top - 0.30), (0.50, top), (-0.50, top)],
                               x - 0.09, x + 0.09, "X", DARK))
        for side in (-1, 1):
            parts.append(c.bar((x, side * 0.74, pad), (x, side * 0.46, top - 0.25), 0.12, 0.12, DARK))
            parts.append(c.box((0.26, 0.24, 0.05), (x, side * 0.74, pad + 0.025), DARK))
        parts.append(c.bar((x, -0.66, pad + 0.35), (x, 0.50, top - 0.32), 0.05, 0.06, DARK))
        parts.append(c.bar((x, 0.66, pad + 0.35), (x, -0.50, top - 0.32), 0.05, 0.06, DARK))
    for side in (-1, 1):                              # ties between the two frames
        parts.append(c.box((1.16, 0.06, 0.06), (0, side * 0.60, pad + 0.75), DARK))
    # manhole and vent on top (highest point Z = 3.0)
    parts.append(c.cylinder(0.24, 0.16, (-0.35, 0, zc + r + 0.04), "Z", GREY, segments=12, cap_bottom=False))
    parts.append(c.cylinder(0.27, 0.04, (-0.35, 0, zc + r + 0.14), "Z", ACCENT, segments=12))
    parts.append(c.cylinder(0.07, 0.26, (0.55, 0, zc + r + 0.09), "Z", GREY, segments=8, cap_bottom=False))
    parts.append(c.cone(0.14, 0.05, 0.08, (0.55, 0, 2.96), "Z", DARK, segments=8))
    # level gauge on the +X end, drain pipe with valve underneath
    parts.append(c.box((0.05, 0.12, 0.80), (half - 0.045, 0, zc), DARK))
    parts.append(c.box((0.03, 0.07, 0.70), (half - 0.02, 0, zc), GLASS))
    parts.append(c.cylinder(0.08, zc - r - 0.60 + 0.06, (0, 0, (zc - r + 0.06 + 0.60) / 2), "Z", GREY,
                            segments=8, cap_bottom=False, cap_top=False))
    parts.append(c.box((0.26, 0.26, 0.20), (0, 0, 0.62), ACCENT))
    parts.append(c.cylinder(0.07, 0.36, (0.30, 0, 0.62), "X", GREY, segments=8, cap_bottom=False))
    parts.append(c.cylinder(0.10, 0.05, (0.50, 0, 0.62), "X", DARK, segments=8))

    c.assert_outward(parts)
    obj = c.join(parts, name)
    c.export(obj, name)
    c.render_previews(obj, name)
    return obj


if __name__ == "__main__":
    build_plant()
    build_addon_a()
    build_addon_b()
