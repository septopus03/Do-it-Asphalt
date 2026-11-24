"""SM_ToolShop, SM_PaverDepot and SM_Garage - the three buildings where upgrades are bought.

All three face +X: the player (or the car) comes from the +X side. Each carries an EMPTY
white board facing +X for the game to write its name on (like SM_Kiosk).

SM_ToolShop (hand tools: bowl, bucket, canister, tub, trolley, cart)
  A yellow site container, 2.4 x 6.0 m, standing on six feet (floor at Z = 0.16, roof at
  Z = 2.74). In the +X wall a serving hatch (Y -2.5 .. -0.1, Z 1.0 .. 2.1) lined dark,
  with wares on show, a steel counter in front of it (top at Z = 1.04) and its flap
  swung up as an orange awning. Right of the hatch a steel door with a step. On the roof
  the board (2.4 x 0.5 m, centre (1.16, -1.3, 3.12)) and a giant bucket of asphalt,
  whose handle is the highest point (Z = 4.0).
  Origin: centre of the container's footprint, on the ground.
  COUNTER = (1.62, -1.3, 1.04): middle of the counter's front edge (where the player stands).
  Envelope: X [-1.24, 2.29], Y +-3.04, Z [0, 4.0]. 5 materials.

SM_PaverDepot (the robots: buying robots and their upgrades)
  An open shelter on a 3.5 x 6.2 m concrete pad: white back and left wall, a white roof
  rising towards the front on four dark posts. Under it three docks side by side, open
  to +X, 1.6 m apart: each has an orange service unit at the back with a filler arm, two
  guide rails 1.35 m apart and floor marks. Right of the docks a white control cabinet
  with a mast (dish, orange beacon: the highest point, Z = 3.41) and, in front of it, an
  orange console with a glass screen facing +X. The board (2.4 x 0.45 m, centre
  (1.74, -0.6, 2.98)) stands on the front edge of the roof.
  Origin: centre of the pad, on the ground.
  DOCKS = (-0.35, -2.2), (-0.35, -0.6), (-0.35, 1.0): where a parked robot's origin goes
  (it faces +X). The game shows SM_AutoPaver at 1.5 x its model size: 1.5 x 1.2 x 0.9 m.
  CONSOLE = (0.95, 2.45): centre of the console's footprint.
  Envelope: X +-1.75, Y +-3.1, Z [0, 3.41]. 5 materials.

SM_Garage (the cars and their upgrades) - DRIVE-IN
  A steel hall 11.0 x 7.7 m under a dark gable roof (ridge along X, top at Z = 5.39,
  eaves at Y = +-4.0), with two vents on the ridge (Z = 5.59). The +X wall has the door:
  6.4 m wide, 4.1 m high, hazard-striped jambs; the yellow roller shutter hangs rolled
  up with its bottom bar at Z = 3.70. It is sized for the biggest truck, SM_PaverCar_T4
  (8.2 x 5.0 x 3.4 m): inside the walls are 7.3 m apart and 10.6 m long, and the floor
  has a yellow parking bay (8.8 x 5.6 m). Along the back wall a workbench under a
  pegboard, a tool cabinet, a rack and two stacks of tyres; two drums by the door; two
  lamps under the ridge. On the inside of the right (+Y) wall, by the door, a terminal
  (dark box, white screen) at TERMINAL = (4.30, 3.55), 1.45 m up, facing -Y: where the
  player buys. In front a 1 m concrete apron with a hazard band and two
  bollards. The board (3.0 x 0.46 m, centre (5.57, 0, 4.56)) is above the door.
  Origin: centre of the parking bay, on the ground - a car parks with its origin here,
  facing +X (out of the door).
  Collision cannot be one convex hull (the car and the player must get in): the boxes
  are in manifest.json ("collision").
  Envelope: X [-5.65, 6.5], Y +-4.0, Z [0, 5.59]. 5 materials.

Run:  blender -b -P Tools/ModelGen/shop_buildings.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

YELLOW, ORANGE, STEEL, DARK = "DIA_Yellow", "DIA_Orange", "DIA_Steel", "DIA_DarkSteel"
WHITE, GLASS, CONCRETE = "DIA_White", "DIA_Glass", "DIA_Concrete"


# --------------------------------------------------------------------------- shared helpers


def frange(lo, hi, step):
    """lo, lo + step, ... up to and including hi (within rounding)."""
    return [round(lo + i * step, 6) for i in range(int(round((hi - lo) / step)) + 1)]


def zx(points_xz):
    """(x, z) pairs -> the (z, x) order c.extrude() wants for a sweep along Y."""
    return [(z, x) for x, z in points_xz]


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


def hazard_band(along, a0, a1, b0, b1, z0, z1, mats, stripe=0.5):
    """Flat band of 45 degree stripes in the two materials `mats`. `along` = 'X' or 'Y'
    is the direction the band runs in, a0 .. a1 its extent that way, b0 .. b1 across."""
    parts = []
    slant = b1 - b0
    k = 0
    while a0 - slant + k * stripe < a1:
        a = a0 - slant + k * stripe
        poly = clip([(a, b0), (a + stripe, b0), (a + stripe + slant, b1), (a + slant, b1)], 0, a0, a1)
        area = abs(sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
                       for i in range(len(poly)))) / 2 if poly else 0.0
        if area > 1e-6:
            if along == "Y":
                poly = [(b, a_) for a_, b in poly]
            parts.append(c.extrude(poly, z0, z1, "Z", mats[k % 2], cap_lo=False))
        k += 1
    return parts


def sign_board(front_x, y, z, width, height):
    """The empty white board the game writes on: front face at X = front_x, facing +X,
    centre (y, z), on a dark backing plate that shows 5 cm around it."""
    return [c.box((0.04, width, height), (front_x - 0.02, y, z), WHITE),
            c.box((0.03, width + 0.10, height + 0.10), (front_x - 0.05, y, z), DARK)]


def report(obj, name):
    mn, mx = c.bounds(obj)
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print("BUILT %s min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f) tris=%d materials=%d" % (
        name, mn[0], mn[1], mn[2], mx[0], mx[1], mx[2], tris, len(obj.data.materials)))
    return obj


# --------------------------------------------------------------------------- SM_ToolShop


def build_tool_shop():
    name = "SM_ToolShop"
    c.reset_scene()
    parts = []

    hx, hy = 1.20, 3.00                  # half depth and half width of the container's skin
    z0, z1 = 0.16, 2.72                  # underside and roof skin
    zm, zh = (z0 + z1) / 2, z1 - z0
    split = 0.68                         # the hatch recess reaches back to this X
    hatch_y, hatch_z = (-2.5, -0.1), (1.0, 2.1)
    hatch_cy = (hatch_y[0] + hatch_y[1]) / 2
    hatch_w = hatch_y[1] - hatch_y[0]
    door_y = 1.65                        # centre of the door

    for x in (-0.95, 0.95):              # feet
        for y in (-2.7, 0.0, 2.7):
            parts.append(c.box((0.30, 0.30, z0 + 0.02), (x, y, (z0 + 0.02) / 2), DARK))

    # Skin: one box behind the recess, four blocks around the hatch in front of it.
    parts.append(c.box((split + hx, 2 * hy, zh), ((split - hx) / 2, 0, zm), YELLOW))
    d, fx = hx - split, (hx + split) / 2
    parts.append(c.box((d, hatch_y[0] + hy, zh), (fx, (hatch_y[0] - hy) / 2, zm), YELLOW))
    parts.append(c.box((d, hy - hatch_y[1], zh), (fx, (hy + hatch_y[1]) / 2, zm), YELLOW))
    parts.append(c.box((d, hatch_w, hatch_z[0] - z0), (fx, hatch_cy, (hatch_z[0] + z0) / 2), YELLOW))
    parts.append(c.box((d, hatch_w, z1 - hatch_z[1]), (fx, hatch_cy, (z1 + hatch_z[1]) / 2), YELLOW))

    # Dark lining of the recess: an open box turned to face +X, its rim 2 cm proud of the wall.
    liner = c.tub((hatch_z[1] - hatch_z[0], hatch_w), (hatch_z[1] - hatch_z[0], hatch_w),
                  0.0, d + 0.02, 0.03, 0.03, (0, 0), DARK)
    c.rotate(liner, "Y", 90)
    parts.append(c.translate(liner, (split, hatch_cy, (hatch_z[0] + hatch_z[1]) / 2)))
    shelf_z = hatch_z[0] + 0.03          # inner floor of the recess

    # Container frame: corner posts, top and bottom rails, 4 cm proud of the skin.
    for sx in (1, -1):
        for sy in (1, -1):
            parts.append(c.box((0.14, 0.14, zh + 0.02), (sx * 1.17, sy * 2.97, zm + 0.01), DARK))
        for z in (z0 + 0.06, z1 - 0.04):
            parts.append(c.box((0.12, 2 * hy + 0.08, 0.12), (sx * 1.18, 0, z), DARK))
            parts.append(c.box((2 * hx + 0.08, 0.12, 0.12), (0, sx * 2.98, z), DARK))

    # Corrugation: ribs standing 2.5 cm off the skin.
    rib_z, rib_h = (0.32 + 2.58) / 2, 2.58 - 0.32
    for y in frange(-2.7, 2.7, 0.3):
        parts.append(c.box((0.05, 0.12, rib_h), (-hx, y, rib_z), YELLOW))
    for y in (-2.72, 0.15, 0.45, 0.75, 2.45, 2.72):
        parts.append(c.box((0.05, 0.12, rib_h), (hx, y, rib_z), YELLOW))
    for x in frange(-0.9, 0.9, 0.3):
        for sy in (1, -1):
            parts.append(c.box((0.12, 0.05, rib_h), (x, sy * hy, rib_z), YELLOW))

    # Counter on two braces, and the flap swung up as an awning on two struts.
    parts.append(c.box((0.44, 2.6, 0.06), (1.40, hatch_cy, 1.01), STEEL))
    for y in (-2.4, -0.2):
        parts.append(c.bar((1.58, y, 0.99), (1.22, y, 0.55), 0.04, 0.04, DARK))
    hinge, tip = (1.22, 2.14), (2.26, 2.42)
    parts.append(c.bar((hinge[0], hatch_cy, hinge[1]), (tip[0], hatch_cy, tip[1]), hatch_w + 0.04, 0.05, ORANGE))
    slope = (tip[1] - hinge[1]) / (tip[0] - hinge[0])
    parts.append(c.bar((2.19, hatch_cy, hinge[1] + (2.19 - hinge[0]) * slope),
                       (2.28, hatch_cy, hinge[1] + (2.28 - hinge[0]) * slope), hatch_w + 0.08, 0.08, DARK))
    for y in (hatch_y[0] + 0.04, hatch_y[1] - 0.04):
        parts.append(c.beam((1.23, y, 1.80), (1.95, y, hinge[1] + (1.95 - hinge[0]) * slope - 0.02),
                            0.02, STEEL, segments=6))

    # Wares in the hatch: bucket, canister, bowls, a till; a shelf with boxes behind them.
    parts.append(c.lathe([(0.10, 0.0), (0.14, 0.27), (0.125, 0.27), (0.09, 0.02), (0.0, 0.02)],
                         (1.00, -2.10, shelf_z), "Z", STEEL, segments=8))
    parts.append(c.box((0.16, 0.26, 0.32), (0.98, -1.55, shelf_z + 0.16), ORANGE, bevel=0.03))
    parts.append(c.cylinder(0.035, 0.05, (0.98, -1.47, shelf_z + 0.34), "Z", DARK, segments=8, cap_bottom=False))
    parts.append(c.box((0.04, 0.12, 0.03), (0.98, -1.61, shelf_z + 0.335), DARK))
    for i in range(3):
        parts.append(c.lathe([(0.06, 0.0), (0.125, 0.07), (0.115, 0.07), (0.055, 0.015), (0.0, 0.015)],
                             (1.00, -0.95, shelf_z + 0.03 * i), "Z", STEEL, segments=8))
    parts.append(c.box((0.14, 0.20, 0.20), (1.05, -0.42, shelf_z + 0.10), DARK))
    parts.append(c.box((0.012, 0.14, 0.10), (1.124, -0.42, shelf_z + 0.12), WHITE))
    parts.append(c.box((0.22, 2.2, 0.03), (0.82, hatch_cy, 1.62), STEEL))
    parts.append(c.box((0.14, 0.30, 0.20), (0.82, -2.05, 1.735), ORANGE))
    parts.append(c.box((0.14, 0.25, 0.25), (0.82, -1.55, 1.76), WHITE))
    parts.append(c.box((0.14, 0.40, 0.16), (0.82, -0.95, 1.715), YELLOW))
    parts.append(c.cylinder(0.08, 0.18, (0.82, -0.45, 1.725), "Z", STEEL, segments=8, cap_bottom=False))

    # Door on the bottom rail, with a handle, a plate, a step and a lamp above.
    parts.append(c.box((0.04, 1.06, 1.98), (1.21, door_y, 1.27), DARK))
    parts.append(c.box((0.03, 0.90, 1.90), (1.235, door_y, 1.25), STEEL))
    parts.append(c.box((0.04, 0.05, 0.18), (1.265, door_y - 0.35, 1.25), DARK))
    parts.append(c.box((0.012, 0.40, 0.25), (1.254, door_y, 1.75), ORANGE))
    parts.append(c.box((0.36, 1.00, 0.14), (1.42, door_y, 0.07), DARK))
    parts.append(c.box((0.12, 0.22, 0.06), (1.28, door_y, 2.40), DARK))
    parts.append(c.box((0.08, 0.16, 0.02), (1.28, door_y, 2.36), WHITE))

    # Roof: the board over the hatch, a vent box and the giant bucket.
    parts += sign_board(1.16, hatch_cy, 3.12, 2.4, 0.5)
    for dy in (-0.9, 0.9):
        parts.append(c.box((0.06, 0.06, 0.16), (1.11, hatch_cy + dy, 2.78), DARK))
    parts.append(c.box((0.50, 0.60, 0.26), (-0.50, -0.30, z1 + 0.13), STEEL, bevel=0.02))
    bx, by, bz = -0.10, 1.70, 2.84
    parts.append(c.cylinder(0.30, bz - z1, (bx, by, (bz + z1) / 2), "Z", DARK, segments=12, cap_bottom=False))
    parts.append(c.lathe([(0.34, 0.0), (0.46, 0.72), (0.43, 0.72), (0.41, 0.62), (0.0, 0.62)],
                         (bx, by, bz), "Z", STEEL, segments=12))
    parts.append(c.cylinder(0.405, 0.03, (bx, by, bz + 0.635), "Z", DARK, segments=12, cap_bottom=False))
    parts.append(c.lathe([(0.402, 0.30), (0.427, 0.45)], (bx, by, bz), "Z", ORANGE, segments=12,
                         cap_start=False, cap_end=False))
    arch_z, r_out, r_in, steps = bz + 0.66, 0.50, 0.46, 10
    angles = [math.pi * i / steps for i in range(steps + 1)]
    outline = ([(by + r_out, bz + 0.58)] + [(by + r_out * math.cos(a), arch_z + r_out * math.sin(a)) for a in angles]
               + [(by - r_out, bz + 0.58), (by - r_in, bz + 0.58)]
               + [(by + r_in * math.cos(a), arch_z + r_in * math.sin(a)) for a in reversed(angles)]
               + [(by + r_in, bz + 0.58)])
    parts.append(c.extrude(outline, bx - 0.02, bx + 0.02, "X", DARK))
    for sy in (1, -1):
        parts.append(c.box((0.07, 0.07, 0.09), (bx, by + sy * 0.465, bz + 0.60), DARK))

    return report(c.finish(parts, name), name)


# --------------------------------------------------------------------------- SM_PaverDepot

DOCKS = ((-0.35, -2.2), (-0.35, -0.6), (-0.35, 1.0))
CONSOLE = (0.95, 2.45)
DOCK_CLEAR = 1.35                     # between the guide rails; the game shows the robot 1.2 m wide


def build_paver_depot():
    name = "SM_PaverDepot"
    c.reset_scene()
    parts = []

    pad = 0.06
    half_y = 3.10                         # half width of the pad and the roof
    px = 1.58                             # posts at X = +-px
    py = half_y - 0.12                    # and Y = +-py
    docks_y = (DOCKS[0][1] + DOCKS[-1][1]) / 2

    def roof_z(x):
        """Middle of the roof slab above X = x (it rises towards the front)."""
        return 2.24 + (x + 1.75) * 0.36 / 3.5

    parts.append(c.box((3.5, 2 * half_y, pad), (0, 0, pad / 2), CONCRETE))

    # Roof on four posts, a beam along each edge.
    parts.append(c.bar((-1.75, 0, roof_z(-1.75)), (1.75, 0, roof_z(1.75)), 2 * half_y, 0.08, WHITE))
    parts.append(c.box((0.03, 2 * half_y, 0.11), (1.745, 0, roof_z(1.75)), ORANGE))        # fascia
    for x in (px, -px):
        top = roof_z(x) - 0.03
        for sy in (1, -1):
            parts.append(c.box((0.12, 0.12, top - pad), (x, sy * py, (top + pad) / 2), DARK))
        parts.append(c.box((0.10, 2 * py + 0.12, 0.12), (x, 0, roof_z(x) - 0.095), DARK))
    for sy in (1, -1):
        parts.append(c.bar((-px, sy * py, roof_z(-px) - 0.10), (px, sy * py, roof_z(px) - 0.10), 0.10, 0.12, DARK))

    # Back wall and left wall (the right side stays open: cabinet and console stand there).
    wall_top = 2.12
    parts.append(c.box((0.06, 2 * py + 0.12, wall_top - pad), (-1.67, 0, (wall_top + pad) / 2), WHITE))
    for y in frange(-2.8, 2.8, 0.4):
        parts.append(c.box((0.04, 0.10, wall_top - 0.26), (-1.70, y, (wall_top + 0.26) / 2), WHITE))
    parts.append(c.box((0.02, 2 * py - 0.16, 0.15), (-1.635, 0, 1.50), ORANGE))
    side = [(-1.52, pad), (1.52, pad), (1.52, roof_z(1.52) - 0.15), (-1.52, roof_z(-1.52) - 0.15)]
    parts.append(c.extrude(zx(side), -py - 0.04, -py + 0.02, "Y", WHITE))
    stripe = [(-1.50, 1.42), (1.50, 1.42), (1.50, 1.57), (-1.50, 1.57)]
    parts.append(c.extrude(zx(stripe), -py - 0.06, -py + 0.04, "Y", ORANGE))

    # Three docks.
    for dock_x, yc in DOCKS:
        parts.append(c.box((0.36, 1.20, 0.78), (-1.44, yc, pad + 0.39), ORANGE, bevel=0.03))
        parts.append(c.box((0.02, 0.70, 0.28), (-1.255, yc, 0.42), DARK))
        parts.append(c.box((0.02, 0.80, 0.06), (-1.255, yc, 0.70), GLASS))
        parts.append(c.bar((-1.44, yc, 0.82), (-0.80, yc, 1.30), 0.07, 0.07, DARK))
        parts.append(c.cylinder(0.05, 0.24, (-0.80, yc, 1.21), "Z", ORANGE, segments=8))
        for sy in (1, -1):
            parts.append(c.box((2.2, 0.05, 0.06), (-0.15, yc + sy * (DOCK_CLEAR + 0.05) / 2, pad + 0.03), DARK))
        parts.append(c.box((0.08, DOCK_CLEAR - 0.05, 0.012), (-1.18, yc, pad + 0.006), WHITE))
        parts.append(c.box((0.30, DOCK_CLEAR - 0.05, 0.012), (1.25, yc, pad + 0.006), ORANGE))

    # Control cabinet with the mast: dish, feed, beacon.
    cx, cy = -1.30, CONSOLE[1]
    cab_top = pad + 1.80
    parts.append(c.box((0.60, 0.90, 1.80), (cx, cy, pad + 0.90), WHITE, bevel=0.02))
    parts.append(c.box((0.02, 0.60, 0.25), (cx + 0.30, cy, 0.45), DARK))
    parts.append(c.box((0.02, 0.60, 0.06), (cx + 0.30, cy, 0.85), ORANGE))
    parts.append(c.box((0.02, 0.50, 0.30), (cx + 0.30, cy, 1.45), GLASS))
    mast_top = 3.30
    parts.append(c.cylinder(0.04, mast_top - cab_top, (cx, cy, (mast_top + cab_top) / 2), "Z", DARK,
                            segments=8, cap_bottom=False))
    parts.append(c.cylinder(0.09, 0.08, (cx, cy, roof_z(cx) + 0.07), "Z", DARK, segments=8))
    parts.append(c.lathe([(0.03, 0.0), (0.30, 0.12), (0.28, 0.12), (0.0, 0.03)], (cx + 0.03, cy, 2.85), "X",
                         WHITE, segments=12))
    parts.append(c.beam((cx + 0.06, cy, 2.85), (cx + 0.28, cy, 2.85), 0.015, DARK, segments=6))
    parts.append(c.cylinder(0.035, 0.05, (cx + 0.30, cy, 2.85), "X", ORANGE, segments=8))
    parts.append(c.cylinder(0.07, 0.04, (cx, cy, mast_top + 0.02), "Z", DARK, segments=8))
    parts.append(c.dome(0.055, 0.07, (cx, cy, mast_top + 0.04), "Z", ORANGE, segments=8, rings=2, cap=False))

    # Console: a pedestal with a sloping glass screen facing +X and up.
    kx, ky = CONSOLE
    parts.append(c.box((0.50, 0.60, 0.04), (kx, ky, pad + 0.02), DARK))
    parts.append(c.box((0.16, 0.24, 0.90), (kx - 0.08, ky, pad + 0.45), DARK))
    low, high = (kx + 0.30, 1.10), (kx - 0.02, 1.52)
    body = [(kx - 0.22, 0.92), (kx + 0.12, 0.92), (kx + 0.30, 1.04), low, high, (kx - 0.22, 1.52)]
    parts.append(c.extrude(zx(body), ky - 0.30, ky + 0.30, "Y", ORANGE))
    length = math.hypot(high[0] - low[0], high[1] - low[1])
    ux, uz = (high[0] - low[0]) / length, (high[1] - low[1]) / length

    def on_panel(s, lift):
        return (low[0] + ux * s + uz * lift, low[1] + uz * s - ux * lift)

    for s0, s1, half, lift, mat in ((0.03, 0.50, 0.27, 0.008, DARK), (0.08, 0.45, 0.22, 0.016, GLASS)):
        plate = [on_panel(s0, 0.0), on_panel(s1, 0.0), on_panel(s1, lift), on_panel(s0, lift)]
        parts.append(c.extrude(zx(plate), ky - half, ky + half, "Y", mat))

    # The board on the front edge of the roof, over the docks.
    parts += sign_board(1.74, docks_y, 2.98, 2.4, 0.45)
    for dy in (-0.9, 0.9):
        parts.append(c.box((0.06, 0.06, 0.16), (1.69, docks_y + dy, roof_z(1.69) + 0.08), DARK))

    return report(c.finish(parts, name), name)


# --------------------------------------------------------------------------- SM_Garage


GARAGE_TERMINAL = (4.30, 3.55)        # wall terminal inside the garage, facing -Y


def build_garage():
    name = "SM_Garage"
    c.reset_scene()
    parts = []

    wall_y, inner_y, eave_y = 3.85, 3.65, 4.00     # outer and inner wall face, roof edge
    back, front = -5.5, 5.5                        # outer faces of the back and front wall
    thick = 0.2
    door_y, door_z = 3.2, 4.1                      # half width and height of the opening
    shutter_z = 3.70                               # underside of the rolled-up shutter
    ridge = 5.25                                   # underside of the roof slab at the ridge
    floor = 0.04

    def under(y):
        """Underside of the roof slab at Y = y."""
        return ridge - 0.85 / 3.0 * abs(y)

    # Roof: a gable slab, ridge along X, overhanging the walls all round.
    roof = [(-eave_y, under(eave_y) + 0.14), (0, ridge + 0.14), (eave_y, under(eave_y) + 0.14),
            (eave_y, under(eave_y)), (0, ridge), (-eave_y, under(eave_y))]
    parts.append(c.extrude(roof, back - 0.15, front + 0.15, "X", DARK))
    for x in (-2.6, 2.6):
        parts.append(c.box((0.50, 0.50, 0.20), (x, 0, ridge + 0.10), STEEL))
        parts.append(c.cylinder(0.20, 0.14, (x, 0, ridge + 0.27), "Z", STEEL, segments=8, cap_bottom=False))

    # Walls: each reaches 2 cm into the roof slab.
    top = under(wall_y) + 0.02
    for s in (1, -1):
        outline = [(s * inner_y, 0), (s * wall_y, 0), (s * wall_y, top), (s * inner_y, under(inner_y) + 0.02)]
        parts.append(c.extrude(outline, back + thick, front - thick, "X", STEEL))
    gable = [(-wall_y, 0), (wall_y, 0), (wall_y, top), (0, ridge + 0.02), (-wall_y, top)]
    parts.append(c.extrude(gable, back, back + thick, "X", STEEL))
    doorway = [(-wall_y, 0), (-door_y, 0), (-door_y, door_z), (door_y, door_z), (door_y, 0), (wall_y, 0),
               (wall_y, top), (0, ridge + 0.02), (-wall_y, top)]
    parts.append(c.extrude(doorway, front - thick, front, "X", STEEL))

    # Cladding: dark corner posts and plinth, ribs on the side and back walls.
    post_top = under(wall_y + 0.03) - 0.01
    rib_top = under(wall_y + 0.02) - 0.02
    for sx in (1, -1):
        for sy in (1, -1):
            parts.append(c.box((0.26, 0.26, post_top), (sx * (front - 0.10), sy * (wall_y - 0.10), post_top / 2), DARK))
    for s in (1, -1):
        parts.append(c.box((2 * (front - 0.23), 0.06, 0.45), (0, s * wall_y, 0.225), DARK))
        parts.append(c.box((0.06, 0.22, 0.45), (front, s * (door_y + 0.31), 0.225), DARK))
        for x in frange(-4.8, 4.8, 0.4):
            parts.append(c.box((0.10, 0.04, rib_top - 0.45), (x, s * wall_y, (rib_top + 0.45) / 2), STEEL))
    parts.append(c.box((0.06, 2 * (wall_y - 0.23), 0.45), (back, 0, 0.225), DARK))
    for y in frange(-3.2, 3.2, 0.4):
        gable_rib = under(abs(y) + 0.05) - 0.03
        parts.append(c.box((0.04, 0.10, gable_rib - 0.45), (back, y, (gable_rib + 0.45) / 2), STEEL))

    # Door: hazard-striped jambs, a yellow head, the shutter rolled up behind it.
    trim_z = door_z - 0.005
    blocks = 10
    for s in (1, -1):
        for i in range(blocks):
            parts.append(c.box((0.06, 0.21, trim_z / blocks),
                               (front, s * (door_y + 0.095), trim_z * (i + 0.5) / blocks), (YELLOW, DARK)[i % 2]))
    parts.append(c.box((0.06, 2 * (door_y + 0.20), 0.165), (front, 0, trim_z + 0.0825), YELLOW))
    parts.append(c.box((0.30, 2 * door_y + 0.20, 0.30), (front - 0.35, 0, door_z + 0.05), DARK))
    parts.append(c.box((0.05, 2 * door_y - 0.02, door_z + 0.04 - shutter_z - 0.02),
                       (front - 0.10, 0, (door_z + 0.04 + shutter_z + 0.02) / 2), YELLOW))
    for z in (shutter_z + 0.12, shutter_z + 0.22, shutter_z + 0.32):
        parts.append(c.box((0.07, 2 * door_y - 0.02, 0.02), (front - 0.10, 0, z), DARK))
    parts.append(c.box((0.09, 2 * door_y - 0.02, 0.08), (front - 0.10, 0, shutter_z + 0.04), DARK))

    parts += sign_board(front + 0.07, 0, 4.56, 3.0, 0.46)

    # Floor, apron with a hazard band, parking bay.
    parts.append(c.box((front - back - 0.1, 2 * inner_y, floor), ((front + back + 0.1) / 2, 0, floor / 2), CONCRETE))
    parts.append(c.box((1.0, 2 * eave_y, floor), (front + 0.5, 0, floor / 2), CONCRETE))
    parts.append(c.box((0.10, 5.6, 0.02), (-4.45, 0, floor), YELLOW))
    for s in (1, -1):
        parts.append(c.box((8.8, 0.10, 0.02), (-0.10, s * 2.8, floor), YELLOW))
    parts += hazard_band("Y", -eave_y, eave_y, front + 0.62, front + 0.92, floor - 0.01, floor + 0.01,
                         (YELLOW, DARK))

    # Bollards guarding the door posts.
    for s in (1, -1):
        b = (front + 0.45, s * (door_y + 0.42))
        parts.append(c.cylinder(0.11, 1.0, (b[0], b[1], floor + 0.5), "Z", YELLOW, segments=12,
                                cap_bottom=False, cap_top=False))
        for z in (0.55, 0.85):
            parts.append(c.ring(0.11, 0.12, -0.08, 0.08, (b[0], b[1], z), "Z", DARK, 12))
        parts.append(c.dome(0.11, 0.05, (b[0], b[1], floor + 1.0), "Z", DARK, segments=12, rings=2, cap=False))

    # Along the back wall, from -Y to +Y: workbench under a pegboard, tool cabinet, rack, tyres.
    bench_x, bench_y = back + 0.55, -2.2
    parts.append(c.box((0.65, 2.0, 0.06), (bench_x, bench_y, 0.92), STEEL))
    parts.append(c.box((0.55, 1.9, 0.04), (bench_x, bench_y, 0.35), DARK))
    for x in (bench_x - 0.27, bench_x + 0.27):
        for y in (bench_y - 0.95, bench_y + 0.95):
            parts.append(c.box((0.06, 0.06, 0.89 - floor), (x, y, (0.89 + floor) / 2), DARK))
    parts.append(c.box((0.25, 0.50, 0.22), (bench_x - 0.05, bench_y - 0.55, 1.06), YELLOW, bevel=0.02))
    parts.append(c.box((0.04, 0.24, 0.03), (bench_x - 0.05, bench_y - 0.55, 1.185), DARK))
    parts.append(c.box((0.20, 0.30, 0.16), (bench_x - 0.05, bench_y + 0.45, 1.03), WHITE))
    parts.append(c.box((0.03, 1.8, 0.9), (back + thick + 0.015, bench_y, 1.75), DARK))
    px = back + thick + 0.04
    parts.append(c.box((0.02, 0.06, 0.45), (px, bench_y - 0.65, 1.75), STEEL))            # wrench
    parts.append(c.box((0.02, 0.13, 0.07), (px, bench_y - 0.65, 1.99), STEEL))
    parts.append(c.box((0.02, 0.05, 0.40), (px, bench_y - 0.30, 1.70), YELLOW))           # hammer
    parts.append(c.box((0.03, 0.20, 0.07), (px, bench_y - 0.30, 1.93), STEEL))
    parts.append(c.box((0.015, 0.50, 0.14), (px, bench_y + 0.25, 1.85), STEEL))           # saw
    parts.append(c.box((0.03, 0.12, 0.16), (px, bench_y + 0.55, 1.85), YELLOW))
    parts.append(c.box((0.02, 0.40, 0.05), (px, bench_y + 0.25, 1.52), WHITE))            # level
    cab_x, cab_y = back + 0.50, -0.45
    parts.append(c.box((0.55, 0.90, 1.00), (cab_x, cab_y, floor + 0.50), YELLOW, bevel=0.02))
    parts.append(c.box((0.60, 0.95, 0.04), (cab_x, cab_y, floor + 1.02), STEEL))
    for z in (0.30, 0.52, 0.74):
        parts.append(c.box((0.012, 0.84, 0.025), (cab_x + 0.278, cab_y, z), DARK))
        parts.append(c.box((0.03, 0.40, 0.03), (cab_x + 0.285, cab_y, z + 0.11), STEEL))
    rack_x, rack_y = back + 0.50, 1.0
    for x in (rack_x - 0.22, rack_x + 0.22):
        for y in (rack_y - 0.7, rack_y + 0.7):
            parts.append(c.box((0.05, 0.05, 2.0), (x, y, floor + 1.0), DARK))
    for z in (0.30, 0.85, 1.40, 1.95):
        parts.append(c.box((0.50, 1.46, 0.04), (rack_x, rack_y, z), STEEL))
    for y, z, size, mat in ((rack_y - 0.40, 0.32, (0.36, 0.44, 0.34), YELLOW), (rack_y + 0.35, 0.32, (0.36, 0.50, 0.26), WHITE),
                            (rack_y + 0.30, 0.87, (0.36, 0.60, 0.30), YELLOW), (rack_y - 0.42, 1.42, (0.36, 0.40, 0.28), WHITE),
                            (rack_y + 0.25, 1.42, (0.30, 0.34, 0.22), DARK)):
        parts.append(c.box(size, (rack_x, y, z + 0.02 + size[2] / 2), mat))
    parts.append(c.cylinder(0.13, 0.30, (rack_x, rack_y - 0.38, 1.02), "Z", STEEL, segments=8, cap_bottom=False))
    for (y, count) in ((2.95, 3), (2.05, 2)):
        for i in range(count):
            parts.append(c.lathe([(0.30, 0.0), (0.45, 0.05), (0.45, 0.21), (0.30, 0.26)],
                                 (back + 0.75, y, floor + 0.27 * i), "Z", DARK, segments=12))
        parts.append(c.cylinder(0.22, 0.02, (back + 0.75, y, floor + 0.27 * count), "Z", YELLOW, segments=12,
                                cap_bottom=False))
    # Two drums by the door.
    for (x, y), mat in (((front - 0.70, -(inner_y - 0.35)), YELLOW), ((front - 1.38, -(inner_y - 0.33)), STEEL)):
        parts.append(c.cylinder(0.29, 0.88, (x, y, floor + 0.44), "Z", mat, segments=12, cap_bottom=False))
        for z in (0.33, 0.63):
            parts.append(c.ring(0.29, 0.305, -0.02, 0.02, (x, y, z), "Z", DARK, 12))
    # Lamps under the ridge.
    for x in (-2.6, 2.6):
        parts.append(c.cylinder(0.02, 0.50, (x, 0, ridge - 0.23), "Z", DARK, segments=6))
        parts.append(c.cone(0.28, 0.06, 0.16, (x, 0, ridge - 0.54), "Z", DARK, segments=12, cap_bottom=False))
        parts.append(c.cylinder(0.27, 0.03, (x, 0, ridge - 0.605), "Z", WHITE, segments=12, cap_top=False))

    # Inside, on the right wall by the door: the terminal where the car's upgrades are bought
    # (it stays reachable with the biggest truck parked in the bay).
    tx, tz = GARAGE_TERMINAL[0], 1.45
    parts.append(c.box((0.50, 0.10, 0.70), (tx, inner_y - 0.05, tz), DARK))
    parts.append(c.box((0.34, 0.02, 0.24), (tx, inner_y - 0.105, tz + 0.15), WHITE))
    parts.append(c.box((0.34, 0.02, 0.08), (tx, inner_y - 0.105, tz - 0.12), YELLOW))
    parts.append(c.box((0.10, 0.03, 0.10), (tx, inner_y - 0.11, tz - 0.26), YELLOW))

    # Outside, on the right wall: a downpipe and a switch box.
    pipe_top = under(wall_y + 0.07) - 0.01
    parts.append(c.cylinder(0.05, pipe_top - 0.02, (front - 0.45, wall_y + 0.07, (pipe_top + 0.02) / 2), "Z", DARK,
                            segments=8))
    parts.append(c.box((0.50, 0.12, 0.70), (3.60, wall_y + 0.05, 1.50), DARK))
    parts.append(c.box((0.30, 0.02, 0.20), (3.60, wall_y + 0.115, 1.60), YELLOW))

    return report(c.finish(parts, name, front=(1.0, 0.5, 0.4)), name)


if __name__ == "__main__":
    build_tool_shop()
    build_paver_depot()
    build_garage()
