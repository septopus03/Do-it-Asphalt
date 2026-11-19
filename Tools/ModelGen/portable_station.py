"""SM_PortableStation - towable asphalt refill station, 1.2 x 0.8 x 1.3 m.

Origin: centre of the footprint, on the ground. A yellow tank lying along X on a
dark chassis with two rubber wheels (axle under the middle of the tank), an
A-shaped drawbar to the rear (-X) ending in a tow eye at X = -0.6 with a jockey
leg, and a discharge arm that rises from the front of the tank and reaches
forward to X = +0.6 at chest height: its downward spout has a dark DIA_Asphalt
mouth in a DIA_Orange collar, like the plant's chute.
Envelope: X +-0.6, Y +-0.4, Z [0, 1.3]. 5 materials.

Run:  blender -b -P Tools/ModelGen/portable_station.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_PortableStation"
BODY, FRAME, TYRE, MOUTH, ACCENT = "DIA_Yellow", "DIA_DarkSteel", "DIA_Rubber", "DIA_Asphalt", "DIA_Orange"

WHEEL_R = 0.23
AXLE_X = 0.04
TANK_R = 0.29
TANK_Z = 0.80             # tank axis
TANK_X = (-0.30, 0.38)

c.reset_scene()
parts = []

# Wheels: tyre, yellow rim, dark hub; outer face of the hub at Y = +-0.40.
for side in (1, -1):
    axis = "Y" if side > 0 else "-Y"
    parts.append(c.cylinder(WHEEL_R, 0.11, (AXLE_X, side * 0.335, WHEEL_R), "Y", TYRE, segments=16))
    parts.append(c.cylinder(0.13, 0.006, (AXLE_X, side * 0.393, WHEEL_R), axis, BODY, segments=8,
                            cap_bottom=False))
    parts.append(c.cylinder(0.05, 0.004, (AXLE_X, side * 0.398, WHEEL_R), axis, FRAME, segments=8,
                            cap_bottom=False))
    # mudguard over the tyre
    parts.append(c.extrude([(WHEEL_R + 0.03, AXLE_X - 0.27), (WHEEL_R + 0.03, AXLE_X + 0.27),
                            (WHEEL_R + 0.065, AXLE_X + 0.27), (WHEEL_R + 0.065, AXLE_X - 0.27)],
                           side * 0.27, side * 0.395, "Y", FRAME))
parts.append(c.cylinder(0.03, 0.56, (AXLE_X, 0, WHEEL_R), "Y", FRAME, segments=6,
                        cap_bottom=False, cap_top=False))

# Chassis: two rails, cross members, two saddles under the tank.
for side in (1, -1):
    parts.append(c.box((0.70, 0.05, 0.07), (0.03, side * 0.20, 0.40), FRAME))
    parts.append(c.box((0.05, 0.05, 0.14), (AXLE_X, side * 0.20, 0.30), FRAME))      # spring hanger
for x in (-0.20, 0.28):
    parts.append(c.extrude([(-0.25, 0.43), (0.25, 0.43), (0.17, 0.60), (-0.17, 0.60)],
                           x - 0.04, x + 0.04, "X", FRAME))

# Tank with domed ends, two orange straps, filler cap.
x0, x1 = TANK_X
xm = (x0 + x1) / 2
half = (x1 - x0) / 2
parts.append(c.lathe([(0.0, -half), (0.20, -half + 0.03), (TANK_R, -half + 0.10), (TANK_R, half - 0.10),
                      (0.20, half - 0.03), (0.0, half)], (xm, 0, TANK_Z), "X", BODY, segments=16))
for x in (-0.20, 0.28):
    parts.append(c.ring(TANK_R - 0.01, TANK_R + 0.012, x - 0.03, x + 0.03, (0, 0, TANK_Z), "X", ACCENT, 16))
parts.append(c.cylinder(0.075, 0.05, (-0.08, 0, TANK_Z + TANK_R + 0.015), "Z", FRAME, segments=8,
                        cap_bottom=False))
parts.append(c.cylinder(0.09, 0.022, (-0.08, 0, TANK_Z + TANK_R + 0.05), "Z", ACCENT, segments=8))

# Drawbar: A-frame from the rails to the tow eye at X = -0.6, jockey leg with a foot.
hitch = (-0.50, 0.0, 0.40)
for side in (1, -1):
    parts.append(c.bar((-0.30, side * 0.20, 0.40), hitch, 0.045, 0.06, FRAME))
parts.append(c.box((0.10, 0.07, 0.07), (-0.505, 0, 0.40), FRAME))
parts.append(c.ring(0.022, 0.05, -0.014, 0.014, (-0.55, 0, 0.40), "Z", ACCENT, 8))   # tow eye
parts.append(c.cylinder(0.02, 0.37, (-0.46, 0, 0.215), "Z", FRAME, segments=6, cap_top=False))
parts.append(c.cylinder(0.055, 0.03, (-0.46, 0, 0.015), "Z", FRAME, segments=8))
parts.append(c.box((0.03, 0.10, 0.03), (-0.46, 0.03, 0.47), ACCENT))                 # jockey crank

# Discharge arm: up from the front of the tank, forward, spout pointing down at X = +0.6.
ARM_R = 0.04
elbow = (0.20, 0.0, 1.255)
tip = (0.50, 0.0, 1.255)
parts.append(c.cylinder(0.06, 0.05, (0.20, 0, TANK_Z + TANK_R + 0.015), "Z", FRAME, segments=8,
                        cap_bottom=False))
parts.append(c.beam((0.20, 0, TANK_Z + TANK_R), elbow, ARM_R, FRAME, segments=8, cap_start=False))
parts.append(c.beam((elbow[0] - 0.03, 0, elbow[2]), tip, ARM_R, FRAME, segments=8, cap_end=False))
parts.append(c.box((0.11, 0.11, 0.09), (0.205, 0, 1.255), FRAME))                    # elbow block
parts.append(c.bar((0.36, 0, TANK_Z + TANK_R - 0.03), (0.44, 0, 1.22), 0.03, 0.03, FRAME))   # stay
# spout head: block at the tip, short flared spout down, orange collar, dark mouth
parts.append(c.box((0.16, 0.13, 0.09), (0.52, 0, 1.255), FRAME))
parts.append(c.cone(0.045, 0.075, 0.13, (0.525, 0, 1.15), "-Z", FRAME, segments=8,
                    cap_bottom=False, cap_top=False))
parts.append(c.ring(0.062, 0.075, -0.03, 0.03, (0.525, 0, 1.085), "Z", ACCENT, 8))
parts.append(c.cylinder(0.064, 0.02, (0.525, 0, 1.075), "-Z", MOUTH, segments=8, cap_bottom=False))
# valve lever on the arm
parts.append(c.box((0.03, 0.03, 0.12), (0.36, 0.0, 1.24), ACCENT))

c.assert_outward(parts)
obj = c.join(parts, NAME)
c.export(obj, NAME)
c.render_previews(obj, NAME)
