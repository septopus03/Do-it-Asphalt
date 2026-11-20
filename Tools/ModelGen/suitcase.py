"""SM_Suitcase - the pulled service case that carries 60 l of asphalt, 0.42 x 0.50 x 1.06 m.

Origin: on the ground under the middle of the wheel axle, so the game tilts the
case about the axle (0, +-0.20, 0.05) when it is pulled. The telescopic handle
and the two inset wheels are on the -X side; the case stands on the wheels and
on two rubber feet at the front.
A hard flight case: dark panels in steel edge profiles with steel corner caps.
On the lid a round steel filler hatch (the steel vessel inside shows as its
collar). Low on the front (+X) the dosing flap: a steel chute whose mouth opens
downwards at (0.345, 0, 0.045), worked by an orange lever on the right. A glass
sight strip on the front shows the level. Orange: handle grip, flap lever,
hatch handle.
Envelope: X [-0.05, 0.37], Y +-0.254, Z [0, 1.055]. 5 materials.

Run:  blender -b -P Tools/ModelGen/suitcase.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_Suitcase"
PANEL, EDGE, TYRE, GRIP, GLASS = "DIA_DarkSteel", "DIA_Steel", "DIA_Rubber", "DIA_Orange", "DIA_Glass"

BODY_X = (-0.010, 0.310)
BODY_HALF_Y = 0.240
BODY_Z = (0.060, 0.780)
EDGE_T = 0.028                       # edge profile section
EDGE_OUT = 0.010                     # how far the profiles stand off the panels
WHEEL_R = 0.050

c.reset_scene()
parts = []

x0, x1 = BODY_X
z0, z1 = BODY_Z
xm, zm = (x0 + x1) / 2, (z0 + z1) / 2
parts.append(c.box((x1 - x0, 2 * BODY_HALF_Y, z1 - z0), (xm, 0, zm), PANEL))

# Edge profiles along all twelve edges, standing proud of the panels.
ex0, ex1 = x0 - EDGE_OUT + EDGE_T / 2, x1 + EDGE_OUT - EDGE_T / 2
ey = BODY_HALF_Y + EDGE_OUT - EDGE_T / 2
ez0, ez1 = z0 - EDGE_OUT + EDGE_T / 2, z1 + EDGE_OUT - EDGE_T / 2
inset = 0.002                        # bars end just inside the corner caps
for ex in (ex0, ex1):
    for sy in (1, -1):
        parts.append(c.box((EDGE_T, EDGE_T, ez1 - ez0 - inset), (ex, sy * ey, zm), EDGE))
    for ez in (ez0, ez1):
        parts.append(c.box((EDGE_T, 2 * ey - inset, EDGE_T), (ex, 0, ez), EDGE))
for sy in (1, -1):
    for ez in (ez0, ez1):
        parts.append(c.box((ex1 - ex0 - inset, EDGE_T, EDGE_T), (xm, sy * ey, ez), EDGE))
# Corner caps.
cap = 0.050
for ex in (x0 - EDGE_OUT + cap / 2 - 0.004, x1 + EDGE_OUT - cap / 2 + 0.004):
    for sy in (1, -1):
        for ez in (z0 - EDGE_OUT + cap / 2 - 0.004, z1 + EDGE_OUT - cap / 2 + 0.004):
            parts.append(c.box((cap, cap, cap), (ex, sy * (BODY_HALF_Y + EDGE_OUT - cap / 2 + 0.004), ez), EDGE,
                               bevel=0.008))
# A waist band where the case splits.
parts.append(c.box((x1 - x0 + 0.012, 2 * BODY_HALF_Y + 0.012, 0.022), (xm, 0, 0.52), EDGE))

# Wheels (inset in the rear bottom edge) and front feet.
for sy in (1, -1):
    axis = "Y" if sy > 0 else "-Y"
    parts.append(c.cylinder(WHEEL_R, 0.045, (0, sy * 0.200, WHEEL_R), "Y", TYRE, segments=12))
    parts.append(c.cylinder(0.024, 0.006, (0, sy * 0.2255, WHEEL_R), axis, EDGE, segments=8, cap_bottom=False))
    parts.append(c.box((0.050, 0.060, 0.046), (0.275, sy * 0.185, 0.023), TYRE))

# Telescopic handle: two tubes out of the top at the rear, an orange grip across.
for sy in (1, -1):
    parts.append(c.box((0.030, 0.034, 0.030), (0.022, sy * 0.110, z1 + 0.024), EDGE))
    parts.append(c.cylinder(0.009, 0.245, (0.022, sy * 0.110, z1 + 0.145), "Z", EDGE, segments=6,
                            cap_bottom=False, cap_top=False))
parts.append(c.box((0.034, 0.270, 0.034), (0.022, 0, 1.038), GRIP, bevel=0.008))

# Filler hatch on the lid: steel collar of the inner vessel, lid, orange handle.
parts.append(c.cylinder(0.105, 0.018, (0.175, 0, z1 + 0.014), "Z", EDGE, segments=12, cap_bottom=False))
parts.append(c.cylinder(0.085, 0.012, (0.175, 0, z1 + 0.029), "Z", PANEL, segments=12, cap_bottom=False))
parts.append(c.box((0.020, 0.090, 0.018), (0.175, 0, z1 + 0.044), GRIP))

# Dosing flap low on the front: chute, mouth facing down, lever on the right.
front = x1
parts.append(c.extrude([(0.210, front), (0.062, front), (0.050, front + 0.060), (0.095, front + 0.060)],
                       -0.110, 0.110, "Y", EDGE))
parts.append(c.box((0.050, 0.200, 0.010), (front + 0.033, 0, 0.049), TYRE))             # mouth
parts.append(c.box((0.012, 0.230, 0.030), (front + 0.052, 0, 0.115), PANEL))           # flap hinge bar
parts.append(c.beam((front + 0.020, 0.150, 0.130), (front + 0.045, 0.150, 0.300), 0.008, EDGE, segments=6))
parts.append(c.box((0.026, 0.026, 0.060), (front + 0.047, 0.150, 0.315), GRIP))

# Sight strip on the front.
parts.append(c.box((0.006, 0.036, 0.300), (front + 0.003, -0.150, 0.340), GLASS))
parts.append(c.box((0.008, 0.050, 0.014), (front + 0.004, -0.150, 0.496), EDGE))

c.finish(parts, NAME)
