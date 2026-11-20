"""SM_Cart - the pushed cart that carries 200 l of asphalt, 1.26 x 0.67 x 0.99 m.

Origin: centre of the footprint, on the ground; it is pushed towards +X.
An open steel hopper tub with sloping walls (outer 0.95 x 0.66 m at the rim,
Z = 0.76; 0.66 x 0.44 m at the floor, Z = 0.30; walls and floor 12 mm) on a dark
chassis: two fixed wheels in front, two small castors under the handle. The push
handle rises behind the tub (-X) to an orange grip bar at (-0.63, 0, 0.97).
Under the front of the tub the adjustable outlet: a 0.50 m wide chute that drops
forward to a mouth just above the ground at (0.585, 0, 0.06), a gate plate
across it and the gate lever with an orange grip on the right.
Envelope: X [-0.65, 0.61], Y +-0.335, Z [0, 0.99]. 5 materials.

Run:  blender -b -P Tools/ModelGen/cart.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_Cart"
TUB, FRAME, TYRE, GRIP, MOUTH = "DIA_Steel", "DIA_DarkSteel", "DIA_Rubber", "DIA_Orange", "DIA_Asphalt"

TUB_X = -0.03                        # centre of the tub
TUB_Z = (0.30, 0.76)
RAIL_Y = 0.20
FRONT_AXLE_X, FRONT_R = 0.30, 0.10
CASTOR_X, CASTOR_R = -0.42, 0.07
HANDLE = (-0.63, 0.24, 0.97)         # top of the handle tubes (right-hand one)

c.reset_scene()
parts = []

parts.append(c.tub((0.95, 0.66), (0.66, 0.44), TUB_Z[0], TUB_Z[1], 0.012, 0.012, (TUB_X, 0), TUB))
# A folded lip around the rim.
for sy in (1, -1):
    parts.append(c.box((0.97, 0.022, 0.030), (TUB_X, sy * 0.324, TUB_Z[1] - 0.012), FRAME))
for sx in (1, -1):
    parts.append(c.box((0.022, 0.626, 0.030), (TUB_X + sx * 0.474, 0, TUB_Z[1] - 0.012), FRAME))

# Chassis: two rails under the tub, cross members.
for sy in (1, -1):
    parts.append(c.box((0.90, 0.04, 0.05), (TUB_X, sy * RAIL_Y, 0.274), FRAME))
for x in (-0.36, 0.30):
    parts.append(c.box((0.04, 2 * RAIL_Y - 0.04, 0.04), (x, 0, 0.270), FRAME))

# Front wheels on a through axle.
parts.append(c.cylinder(0.014, 0.56, (FRONT_AXLE_X, 0, FRONT_R), "Y", FRAME, segments=6,
                        cap_bottom=False, cap_top=False))
for sy in (1, -1):
    axis = "Y" if sy > 0 else "-Y"
    parts.append(c.cylinder(FRONT_R, 0.05, (FRONT_AXLE_X, sy * 0.300, FRONT_R), "Y", TYRE, segments=12))
    parts.append(c.cylinder(0.055, 0.006, (FRONT_AXLE_X, sy * 0.328, FRONT_R), axis, TUB, segments=8,
                            cap_bottom=False))
    parts.append(c.box((0.04, 0.04, 0.16), (FRONT_AXLE_X, sy * RAIL_Y, 0.17), FRAME))
# Rear castors: stem, fork plate, cheeks, wheel.
for sy in (1, -1):
    y = sy * RAIL_Y
    parts.append(c.cylinder(0.016, 0.10, (CASTOR_X + 0.03, y, 0.20), "Z", FRAME, segments=6,
                            cap_bottom=False, cap_top=False))
    parts.append(c.box((0.10, 0.07, 0.014), (CASTOR_X + 0.01, y, 0.150), FRAME))
    for cheek in (1, -1):
        parts.append(c.box((0.07, 0.008, 0.09), (CASTOR_X, y + cheek * 0.031, 0.105), FRAME))
    parts.append(c.cylinder(CASTOR_R, 0.04, (CASTOR_X, y, CASTOR_R), "Y", TYRE, segments=10))

# Push handle: two tubes from the rail ends up and back, orange grip bar across.
for sy in (1, -1):
    parts.append(c.box((0.05, 0.09, 0.05), (-0.465, sy * 0.225, 0.274), FRAME))
    parts.append(c.beam((-0.47, sy * HANDLE[1], 0.29), (HANDLE[0], sy * HANDLE[1], HANDLE[2]), 0.014, FRAME,
                        segments=6))
    parts.append(c.beam((-0.50, sy * HANDLE[1], 0.42), (-0.44, sy * HANDLE[1], 0.60), 0.010, FRAME,
                        segments=6))                                                      # brace to the tub
parts.append(c.cylinder(0.018, 0.54, (HANDLE[0], 0, HANDLE[2]), "Y", GRIP, segments=8))

# Outlet: chute from under the tub floor down to the mouth, gate plate, lever.
parts.append(c.bar((0.22, 0, 0.300), (0.565, 0, 0.105), 0.50, 0.07, TUB))
for sy in (1, -1):                                                                        # chute cheeks
    parts.append(c.bar((0.22, sy * 0.256, 0.330), (0.565, sy * 0.256, 0.135), 0.012, 0.11, FRAME))
parts.append(c.box((0.05, 0.50, 0.012), (0.585, 0, 0.062), MOUTH))
parts.append(c.box((0.014, 0.54, 0.13), (0.50, 0, 0.235), FRAME))                          # gate plate
parts.append(c.beam((0.50, 0.290, 0.20), (0.42, 0.290, 0.50), 0.009, FRAME, segments=6))
parts.append(c.box((0.014, 0.05, 0.03), (0.50, 0.275, 0.205), FRAME))
parts.append(c.box((0.030, 0.030, 0.075), (0.413, 0.290, 0.525), GRIP))

c.finish(parts, NAME)
