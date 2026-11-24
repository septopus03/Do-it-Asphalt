"""SM_Trolley - the small pushed paving trolley that carries 80 l of asphalt and
lays a narrow strip while it is pushed, 0.96 x 0.52 x 0.90 m.

The light sibling of SM_Cart (1.26 x 0.67 x 0.99 m, 200 l): shorter, lower, ONE
wheel pair in front and two fixed rest legs with rubber feet at the back instead
of castors, a 0.40 m outlet instead of 0.50 m.

Origin: centre of the footprint, on the ground; it is pushed towards +X.
An open steel hopper tub with sloping walls (outer 0.70 x 0.50 m at the rim,
Z = 0.60; 0.46 x 0.32 m at the floor, Z = 0.26; walls and floor 10 mm) on a dark
chassis: two rubber wheels (radius 0.11) on a through axle at X = 0.20, two rest
legs at X = -0.34. The push handle rises behind the tub (-X) to an orange grip
bar; under the front of the tub the outlet: a 0.40 m wide chute that drops
forward to a mouth just above the ground, a gate plate across it and the gate
lever with an orange grip on the right.

Points the game uses (model space, cm):
  centre of the handle grip      (-48.0, 0, 88.0)   bar along Y, 44 long, radius 1.8
  outlet mouth, on the ground    ( 43.5, 0,  0.0)   mouth plate 5 x 40 at Z 5.2 .. 6.4
  wheel axle                     ( 20.0, +-23.5, 11.0)
Envelope (cm): min (-49.8, -26.0, 0.0), max (46.0, 26.0, 89.8). 5 materials.

Run:  blender -b -P Tools/ModelGen/trolley.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_Trolley"
TUB, FRAME, TYRE, GRIP, MOUTH = "DIA_Steel", "DIA_DarkSteel", "DIA_Rubber", "DIA_Orange", "DIA_Asphalt"

TUB_X = -0.02                        # centre of the tub
TUB_Z = (0.26, 0.60)
RAIL_Y = 0.15
AXLE_X, WHEEL_R, WHEEL_Y = 0.20, 0.11, 0.235
LEG_X = -0.34
HANDLE = (-0.48, 0.19, 0.88)         # top of the handle tubes (right-hand one) = grip axis
MOUTH_X, CHUTE_W = 0.435, 0.40

c.reset_scene()
parts = []

parts.append(c.tub((0.70, 0.50), (0.46, 0.32), TUB_Z[0], TUB_Z[1], 0.010, 0.010, (TUB_X, 0), TUB))
# A folded lip around the rim.
for sy in (1, -1):
    parts.append(c.box((0.716, 0.018, 0.026), (TUB_X, sy * 0.245, TUB_Z[1] - 0.010), FRAME))
for sx in (1, -1):
    parts.append(c.box((0.018, 0.472, 0.026), (TUB_X + sx * 0.349, 0, TUB_Z[1] - 0.010), FRAME))

# Chassis: two rails under the tub, cross members.
for sy in (1, -1):
    parts.append(c.box((0.62, 0.035, 0.04), (-0.05, sy * RAIL_Y, 0.240), FRAME))
for x in (-0.30, 0.08):
    parts.append(c.box((0.035, 2 * RAIL_Y - 0.035, 0.03), (x, 0, 0.238), FRAME))

# The wheel pair on a through axle.
parts.append(c.cylinder(0.013, 0.43, (AXLE_X, 0, WHEEL_R), "Y", FRAME, segments=6,
                        cap_bottom=False, cap_top=False))
for sy in (1, -1):
    axis = "Y" if sy > 0 else "-Y"
    parts.append(c.cylinder(WHEEL_R, 0.044, (AXLE_X, sy * WHEEL_Y, WHEEL_R), "Y", TYRE, segments=12))
    parts.append(c.cylinder(0.060, 0.003, (AXLE_X, sy * (WHEEL_Y + 0.0235), WHEEL_R), axis, TUB, segments=8,
                            cap_bottom=False))
    parts.append(c.box((0.035, 0.035, 0.13), (AXLE_X, sy * RAIL_Y, 0.160), FRAME))

# Rest legs at the back (no castors): splayed legs, rubber feet, a tie bar.
for sy in (1, -1):
    parts.append(c.bar((-0.30, sy * RAIL_Y, 0.225), (LEG_X, sy * 0.17, 0.018), 0.030, 0.030, FRAME,
                       cap_start=False, cap_end=False))
    parts.append(c.box((0.075, 0.045, 0.020), (LEG_X, sy * 0.17, 0.010), TYRE))
parts.append(c.box((0.024, 0.32, 0.024), (-0.322, 0, 0.110), FRAME))

# Push handle: two tubes from the rail ends up and back, a tie tube, orange grip bar across.
for sy in (1, -1):
    parts.append(c.box((0.05, 0.075, 0.04), (-0.345, sy * 0.170, 0.240), FRAME))
    parts.append(c.beam((-0.35, sy * HANDLE[1], 0.25), (HANDLE[0], sy * HANDLE[1], HANDLE[2]), 0.013, FRAME,
                        segments=6))
parts.append(c.cylinder(0.010, 2 * HANDLE[1], (-0.428, 0, 0.63), "Y", FRAME, segments=6,
                        cap_bottom=False, cap_top=False))
parts.append(c.cylinder(0.018, 0.44, (HANDLE[0], 0, HANDLE[2]), "Y", GRIP, segments=8))

# Outlet: chute from under the tub floor down to the mouth, gate plate, lever.
parts.append(c.bar((0.20, 0, 0.262), (0.42, 0, 0.100), CHUTE_W, 0.06, TUB))
for sy in (1, -1):                                                                        # chute cheeks
    parts.append(c.bar((0.20, sy * 0.203, 0.288), (0.42, sy * 0.203, 0.126), 0.010, 0.10, FRAME))
parts.append(c.box((0.05, CHUTE_W, 0.012), (MOUTH_X, 0, 0.058), MOUTH))
parts.append(c.box((0.012, 0.43, 0.12), (0.365, 0, 0.215), FRAME))                         # gate plate
parts.append(c.beam((0.365, 0.228, 0.19), (0.305, 0.228, 0.43), 0.008, FRAME, segments=6))
parts.append(c.box((0.012, 0.03, 0.03), (0.365, 0.220, 0.195), FRAME))
parts.append(c.box((0.028, 0.028, 0.070), (0.299, 0.228, 0.455), GRIP))

obj = c.finish(parts, NAME)
mn, mx = c.bounds(obj)
print("ENVELOPE_CM %s min (%.1f, %.1f, %.1f) max (%.1f, %.1f, %.1f)"
      % ((NAME,) + tuple(v * 100 for v in mn) + tuple(v * 100 for v in mx)))
