"""SM_VehicleStation - fixed filling station for the asphalt vehicles, 8.0 x 5.45 x 4.6 m.

Origin: centre of the pad's footprint, on the ground. Vehicles drive through along X.
  Pad: 8.0 (X) x 5.0 (Y) concrete slab, top at Z = 0.05, with yellow / black hazard
  bands (0.30 wide, top at Z = 0.06) along the two long edges and across both ends.
  Portal gantry at X = 0: two 0.40 m square steel posts at Y = +-2.2 up to the beam,
  beam Z 4.2 .. 4.6 across; clear width between the posts 4.0 m.
  Hopper hanging in the middle of the beam: 2.2 x 2.2 m, Z 3.45 .. 4.6 (straight to
  Z = 3.95, tapered to the outlet at Z = 3.45), DIA_Orange with a yellow band; a thick pipe
  (radius 0.24) below it ends in a yellow nozzle ring (outer radius 0.31, Z 2.9 .. 3.0).
  NOZZLE = (0, 0, 2.9): centre of the mouth, the lowest point over the lane
  (vehicles are 2.4 m tall).
  Control cabinet 0.8 x 0.5 x 1.4 m beside the pad at (-3.0, -2.7), front towards
  the lane (+Y), with a yellow warning light on top (up to Z = 1.56).
Envelope: X +-4.0, Y [-2.95, 2.5], Z [0, 4.6]. 5 materials.

Run:  blender -b -P Tools/ModelGen/vehicle_station.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_VehicleStation"
PAD, STEEL, DARK, BODY, WARN = "DIA_Concrete", "DIA_Steel", "DIA_DarkSteel", "DIA_Orange", "DIA_Yellow"

PAD_X, PAD_Y = 8.0, 5.0
PAD_TOP = 0.05
STRIPE_TOP = 0.06
BAND = 0.30                 # width of a hazard band
STRIPE = 0.50               # length of one colour along the band
POST_Y = 2.2
BEAM_Z = (4.2, 4.6)
NOZZLE = (0.0, 0.0, 2.9)


def clip(poly, axis, lo, hi):
    """Clip a convex polygon [(u, v), ...] to lo <= coordinate[axis] <= hi."""
    for bound, keep_above in ((lo, True), (hi, False)):
        out = []
        for i, p in enumerate(poly):
            q = poly[(i + 1) % len(poly)]
            p_in = p[axis] >= bound if keep_above else p[axis] <= bound
            q_in = q[axis] >= bound if keep_above else q[axis] <= bound
            if p_in:
                out.append(p)
            if p_in != q_in:
                t = (bound - p[axis]) / (q[axis] - p[axis])
                out.append(tuple(p[k] + (q[k] - p[k]) * t for k in range(2)))
        poly = out
        if len(poly) < 3:
            return []
    return poly


def hazard_band(along, a0, a1, b0, b1):
    """Band of 45 degree yellow / black stripes on the pad. `along` = 'X' or 'Y' is
    the direction the band runs in, a0 .. a1 its extent that way, b0 .. b1 across."""
    parts = []
    slant = b1 - b0
    k = 0
    start = a0 - slant
    while start + k * STRIPE < a1:
        a = start + k * STRIPE
        poly = clip([(a, b0), (a + STRIPE, b0), (a + STRIPE + slant, b1), (a + slant, b1)], 0, a0, a1)
        if poly:
            if along == "Y":
                poly = [(b, a) for a, b in poly]
            parts.append(c.extrude(poly, PAD_TOP - 0.03, STRIPE_TOP, "Z", (WARN, DARK)[k % 2],
                                   cap_lo=False))
        k += 1
    return parts


c.reset_scene()
parts = []

# ---------------------------------------------------------------- pad and hazard bands
parts.append(c.box((PAD_X, PAD_Y, PAD_TOP), (0, 0, PAD_TOP / 2), PAD))
hx, hy = PAD_X / 2, PAD_Y / 2
for side in (1, -1):
    y0, y1 = sorted((side * hy, side * (hy - BAND)))
    parts += hazard_band("X", -hx, hx, y0, y1)
    x0, x1 = sorted((side * hx, side * (hx - BAND)))
    parts += hazard_band("Y", -(hy - BAND), hy - BAND, x0, x1)

# ---------------------------------------------------------------- portal gantry
for side in (1, -1):
    y = side * POST_Y
    parts.append(c.box((0.60, 0.60, 0.08), (0, y, STRIPE_TOP + 0.03), DARK))             # base plate
    parts.append(c.box((0.40, 0.40, BEAM_Z[0] + 0.02 - 0.05), (0, y, (BEAM_Z[0] + 0.02 + 0.05) / 2), STEEL))
    # beam half: from the hopper wall out over the post
    parts.append(c.box((0.40, 1.34, BEAM_Z[1] - BEAM_Z[0]),
                       (0, side * (1.06 + 0.67), (BEAM_Z[0] + BEAM_Z[1]) / 2), STEEL))
    # knee brace and a yellow collar under the joint
    parts.append(c.bar((0, side * 2.02, 3.45), (0, side * 1.25, 4.24), 0.16, 0.16, DARK))
    parts.append(c.box((0.46, 0.46, 0.50), (0, y, 1.25), WARN))                          # impact sleeve
    parts.append(c.box((0.47, 0.47, 0.12), (0, y, 1.25), DARK))

# ---------------------------------------------------------------- hopper, pipe, nozzle
half = 1.1
k = math.sqrt(2.0)
hopper = c.lathe([(0.34 * k, 3.45), (half * k, 3.95), (half * k, 4.60)], (0, 0, 0), "Z", BODY, segments=4)
parts.append(c.rotate(hopper, "Z", 45))
band = c.lathe([(1.13 * k, 4.01), (1.13 * k, 4.17)], (0, 0, 0), "Z", WARN, segments=4,
               cap_start=True, cap_end=True)
parts.append(c.rotate(band, "Z", 45))
# lid frame and hatch on top (flush with the top of the beam)
parts.append(c.box((1.20, 1.20, 0.04), (0, 0, 4.58), DARK))
# slide gate under the outlet, with its actuator towards -X
parts.append(c.box((0.80, 0.80, 0.14), (0, 0, 3.43), DARK))
parts.append(c.box((0.55, 0.16, 0.10), (-0.62, 0, 3.43), STEEL))
# pipe and nozzle: the mouth is the underside of the yellow ring
nx, ny, nz = NOZZLE
parts.append(c.cylinder(0.24, 0.42, (nx, ny, 3.17), "Z", DARK, segments=12, cap_top=False))
parts.append(c.ring(0.21, 0.31, 0.0, 0.10, NOZZLE, "Z", WARN, 12))
# hangers from the beam halves down to the gate
for side in (1, -1):
    parts.append(c.bar((0, side * 1.30, 4.22), (0, side * 0.36, 3.46), 0.08, 0.08, DARK))

# ---------------------------------------------------------------- control cabinet
cx, cy = -3.0, -2.7
parts.append(c.box((0.86, 0.50, 0.10), (cx, cy, 0.05), DARK))                            # plinth
parts.append(c.box((0.80, 0.50, 1.30), (cx, cy, 0.75), STEEL, bevel=0.02))
parts.append(c.box((0.60, 0.03, 0.80), (cx, cy + 0.25, 0.80), DARK))                     # door
parts.append(c.box((0.34, 0.03, 0.20), (cx, cy + 0.26, 1.00), WARN))                     # panel
parts.append(c.box((0.10, 0.04, 0.10), (cx + 0.16, cy + 0.26, 0.62), BODY))              # button
parts.append(c.cylinder(0.10, 0.05, (cx, cy, 1.425), "Z", DARK, segments=8, cap_bottom=False))
parts.append(c.cone(0.075, 0.06, 0.11, (cx, cy, 1.505), "Z", WARN, segments=8, cap_bottom=False))

# self-check of the functional point: lowest geometry above the lane is the nozzle
lane = [p for p in parts if c.bounds(p)[0][2] > 1.0 and abs(c.bounds(p)[0][1]) < 1.9
        and abs(c.bounds(p)[1][1]) < 1.9]
assert abs(min(c.bounds(p)[0][2] for p in lane) - nz) < 1e-6

c.finish(parts, NAME)
