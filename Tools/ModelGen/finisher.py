"""SM_Finisher - the walk-behind strip finisher, 1.32 x 1.07 x 1.0 m.

Origin: on the ground under the middle of the front screed bar, which lies along
Y at X = 0 (like the spreader); the machine is led towards +X and everything
else is behind the bar.
Front to back: the steel screed bar, 1.0 m wide, just above the ground between
two dark end plates, with a slanted guard plate rising from it; two rubber
transport wheels at the sides; the yellow engine housing with a dark engine
block, a steel tank with an orange cap and an exhaust on top; the steel
compaction roller (radius 0.16 m, 0.92 m wide) at X = -0.58; two handlebars
rising back to orange grips at (-1.15, +-0.25, 0.98) with a control box and an
orange lever between them.
Envelope: X [-1.22, 0.10], Y +-0.536, Z [0, 1.0]. 5 materials.

Run:  blender -b -P Tools/ModelGen/finisher.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_Finisher"
BODY, FRAME, METAL, TYRE, GRIP = "DIA_Yellow", "DIA_DarkSteel", "DIA_Steel", "DIA_Rubber", "DIA_Orange"

BAR_HALF_Y = 0.50
ROLLER_X, ROLLER_R = -0.58, 0.16
WHEEL_X, WHEEL_R = -0.28, 0.11
BODY_X = (-0.56, -0.10)
BODY_Z = (0.34, 0.64)
GRIP_POS = (-1.15, 0.25, 0.98)

c.reset_scene()
parts = []

# Screed bar with its guard plate and end plates.
parts.append(c.box((0.10, 2 * BAR_HALF_Y, 0.10), (0.0, 0, 0.07), METAL))
parts.append(c.extrude([(0.12, 0.030), (0.12, 0.050), (0.30, 0.100), (0.30, 0.080)],
                       -BAR_HALF_Y, BAR_HALF_Y, "Y", FRAME))
for sy in (1, -1):
    parts.append(c.box((0.24, 0.02, 0.18), (-0.02, sy * (BAR_HALF_Y + 0.01), 0.09), FRAME))
    # arms from the bar back to the side frames
    parts.append(c.bar((-0.02, sy * 0.445, 0.14), (-0.14, sy * 0.445, 0.24), 0.03, 0.06, FRAME))

# Side frames tying bar, wheels and roller together.
for sy in (1, -1):
    parts.append(c.box((0.64, 0.03, 0.10), (-0.42, sy * 0.445, 0.25), FRAME))
    parts.append(c.box((0.06, 0.03, 0.12), (ROLLER_X, sy * 0.445, 0.17), FRAME))          # roller bearing drop

# Transport wheels.
for sy in (1, -1):
    axis = "Y" if sy > 0 else "-Y"
    parts.append(c.cylinder(WHEEL_R, 0.06, (WHEEL_X, sy * 0.500, WHEEL_R), "Y", TYRE, segments=12))
    parts.append(c.cylinder(0.055, 0.006, (WHEEL_X, sy * 0.533, WHEEL_R), axis, BODY, segments=8,
                            cap_bottom=False))
    parts.append(c.box((0.06, 0.03, 0.10), (WHEEL_X, sy * 0.445, 0.16), FRAME))

# Compaction roller with dark hubs and a scraper bar behind it.
parts.append(c.cylinder(ROLLER_R, 0.84, (ROLLER_X, 0, ROLLER_R), "Y", METAL, segments=12))
for sy in (1, -1):
    parts.append(c.cylinder(0.06, 0.014, (ROLLER_X, sy * 0.426, ROLLER_R), "Y", FRAME, segments=8))
parts.append(c.box((0.025, 0.86, 0.035), (ROLLER_X - 0.19, 0, 0.27), FRAME))
for sy in (1, -1):
    parts.append(c.box((0.14, 0.03, 0.03), (ROLLER_X - 0.13, sy * 0.445, 0.27), FRAME))

# Engine housing and what sits on it.
bx = (BODY_X[0] + BODY_X[1]) / 2
bz = (BODY_Z[0] + BODY_Z[1]) / 2
parts.append(c.box((BODY_X[1] - BODY_X[0], 0.66, BODY_Z[1] - BODY_Z[0]), (bx, 0, bz), BODY, bevel=0.03))
for sy in (1, -1):                                                                        # housing mounts
    parts.append(c.box((0.30, 0.10, 0.06), (bx, sy * 0.385, 0.31), FRAME))
    for i in range(3):                                                                    # vents
        parts.append(c.box((0.22, 0.006, 0.026), (bx, sy * 0.331, bz - 0.06 + i * 0.055), FRAME))
parts.append(c.box((0.24, 0.28, 0.13), (-0.38, -0.14, BODY_Z[1] + 0.06), FRAME, bevel=0.015))    # engine block
parts.append(c.cylinder(0.026, 0.24, (-0.22, -0.22, BODY_Z[1] + 0.12), "Z", METAL, segments=8, cap_bottom=False))
parts.append(c.box((0.22, 0.22, 0.09), (-0.36, 0.17, BODY_Z[1] + 0.04), METAL, bevel=0.015))     # tank
parts.append(c.cylinder(0.035, 0.022, (-0.36, 0.17, BODY_Z[1] + 0.094), "Z", GRIP, segments=8, cap_bottom=False))

# Handlebars, cross tube, control box with lever, grips.
for sy in (1, -1):
    y = sy * GRIP_POS[1]
    parts.append(c.beam((-0.52, y, BODY_Z[1] - 0.03), (-1.09, y, GRIP_POS[2]), 0.016, FRAME, segments=6))
    parts.append(c.cylinder(0.021, 0.14, (GRIP_POS[0], y, GRIP_POS[2]), "X", GRIP, segments=8))
parts.append(c.cylinder(0.014, 0.50, (-0.86, 0, 0.823), "Y", FRAME, segments=6, cap_bottom=False, cap_top=False))
parts.append(c.box((0.10, 0.16, 0.07), (-0.90, 0, 0.865), FRAME))
parts.append(c.box((0.02, 0.02, 0.085), (-0.90, 0.045, 0.94), GRIP))

c.finish(parts, NAME)
