"""SM_Kiosk and SM_Lock - the upgrade terminal and the padlock shown on a locked one.

SM_Kiosk (0.6 x 0.8 x 2.2 m; origin = centre of the footprint, on the ground):
  dark base plate and column; at waist-to-chest height a yellow console whose
  sloping top faces +X and up: a DIA_Glass screen in a dark bezel and, below it,
  one large DIA_Green button. Above head height the EMPTY white board, exactly
  0.6 x 0.3 m, centre at Z = 1.9, front face at X = BOARD_FRONT_X facing +X (the
  game draws the text on it). A small yellow canopy closes the top at Z = 2.2.
  Envelope: X +-0.3, Y +-0.4, Z [0, 2.2]. 5 materials.

SM_Lock (0.10 x 0.30 x 0.40 m; origin = centre of the envelope):
  red padlock body with a steel shackle, flat in X so it can hang in front of
  the kiosk screen; a dark keyhole on both faces.
  Envelope: X +-0.05, Y +-0.15, Z +-0.2. 3 materials.

Run:  blender -b -P Tools/ModelGen/kiosk.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

BODY = "DIA_Yellow"
FRAME = "DIA_DarkSteel"
SCREEN = "DIA_Glass"
BUTTON = "DIA_Green"
BOARD = "DIA_White"

# --- kiosk ------------------------------------------------------------------
BASE = (0.60, 0.60, 0.04)            # base plate; its X size is the envelope depth
COLUMN_X = (-0.20, -0.04)            # the column stands behind the board
COLUMN_HALF_Y = 0.12
COLUMN_TOP = 2.13                    # ends inside the canopy

BOARD_SIZE = (0.60, 0.30)            # Y x Z, docs/03 chapter 4.3
BOARD_Z = 1.90                       # centre of the board
BOARD_FRONT_X = 0.0                  # the white face the game writes on
BOARD_THICK = 0.035
BOARD_MARGIN = 0.03                  # dark backing plate showing around the board

CONSOLE_HALF_Y = 0.25
# Console outline in the X-Z plane; the edge SLOPE_LOW -> SLOPE_HIGH is the panel.
SLOPE_LOW = (0.30, 1.10)
SLOPE_HIGH = (-0.02, 1.52)
CONSOLE_XZ = [(-0.22, 0.92), (0.12, 0.92), (0.30, 1.04), SLOPE_LOW, SLOPE_HIGH, (-0.22, 1.52)]

# Positions along the panel, metres up the slope from SLOPE_LOW.
BEZEL_S = (0.03, 0.51)
BUTTON_S = 0.115
SCREEN_S = (0.21, 0.49)
BEZEL_HALF_Y, SCREEN_HALF_Y = 0.23, 0.19
BEZEL_H, SCREEN_H, BUTTON_H = 0.008, 0.016, 0.035   # how far each stands off the panel
BUTTON_R = 0.065

CANOPY_HALF_Y = 0.40                 # the widest part: the envelope width
CANOPY_XZ = [(-0.24, 2.14), (0.06, 2.10), (0.06, 2.14), (-0.24, 2.20)]

# --- lock -------------------------------------------------------------------
LOCK_BODY = (0.092, 0.30, 0.24)      # X x Y x Z
LOCK_BODY_Z = (-0.20, 0.04)
SHACKLE_OUTER, SHACKLE_INNER = 0.10, 0.055
SHACKLE_ARCH_Z = 0.10                # centre of the arch; top = 0.10 + 0.10 = +0.20
SHACKLE_FOOT_Z = 0.02                # the legs end inside the body
SHACKLE_HALF_X = 0.02
ARCH_STEPS = 8
KEYHOLE_Z = -0.06
KEYHOLE_R = 0.032
KEYHOLE_PROUD = 0.004                # body 0.046 + 0.004 = the 0.05 half depth


def zx(points_xz):
    """(x, z) pairs -> the (z, x) order c.extrude() wants for a sweep along Y."""
    return [(z, x) for x, z in points_xz]


def on_panel(s, lift=0.0):
    """(x, z) of the point `s` metres up the sloping panel, `lift` metres off it."""
    dx, dz = SLOPE_HIGH[0] - SLOPE_LOW[0], SLOPE_HIGH[1] - SLOPE_LOW[1]
    length = math.hypot(dx, dz)
    ux, uz = dx / length, dz / length          # up the slope
    nx, nz = uz, -ux                           # out of the panel (+X and up)
    return (SLOPE_LOW[0] + ux * s + nx * lift, SLOPE_LOW[1] + uz * s + nz * lift)


def panel_plate(s0, s1, half_y, lift0, lift1, mat):
    """Flat plate lying on the panel between slope positions s0 and s1."""
    outline = [on_panel(s0, lift0), on_panel(s1, lift0), on_panel(s1, lift1), on_panel(s0, lift1)]
    return c.extrude(zx(outline), -half_y, half_y, "Y", mat)


def build_kiosk():
    name = "SM_Kiosk"
    c.reset_scene()
    parts = []

    parts.append(c.box(BASE, (0, 0, BASE[2] / 2), FRAME))
    col_x = (COLUMN_X[0] + COLUMN_X[1]) / 2
    parts.append(c.box((COLUMN_X[1] - COLUMN_X[0], 2 * COLUMN_HALF_Y, COLUMN_TOP - BASE[2]),
                       (col_x, 0, (COLUMN_TOP + BASE[2]) / 2), FRAME))
    # foot collar where the column meets the plate
    parts.append(c.box((0.26, 0.34, 0.06), (col_x, 0, BASE[2] + 0.03), FRAME))

    # Console with the sloping panel: bezel, screen, button.
    parts.append(c.extrude(zx(CONSOLE_XZ), -CONSOLE_HALF_Y, CONSOLE_HALF_Y, "Y", BODY))
    parts.append(panel_plate(BEZEL_S[0], BEZEL_S[1], BEZEL_HALF_Y, 0.0, BEZEL_H, FRAME))
    parts.append(panel_plate(SCREEN_S[0], SCREEN_S[1], SCREEN_HALF_Y, 0.0, SCREEN_H, SCREEN))
    bx0, bz0 = on_panel(BUTTON_S)
    bx1, bz1 = on_panel(BUTTON_S, BUTTON_H)
    parts.append(c.beam((bx0, 0, bz0), (bx1, 0, bz1), BUTTON_R, BUTTON, segments=12, cap_start=False))

    # Empty white board on a dark backing plate, in front of the column.
    bw, bh = BOARD_SIZE
    parts.append(c.box((BOARD_THICK, bw, bh), (BOARD_FRONT_X - BOARD_THICK / 2, 0, BOARD_Z), BOARD))
    back_thick = 0.02
    parts.append(c.box((back_thick, bw + 2 * BOARD_MARGIN, bh + 2 * BOARD_MARGIN),
                       (COLUMN_X[1] + back_thick / 2, 0, BOARD_Z), FRAME))

    # Canopy: a thin wedge sloping down towards the front, short enough not to hide the board.
    parts.append(c.extrude(zx(CANOPY_XZ), -CANOPY_HALF_Y, CANOPY_HALF_Y, "Y", BODY))

    return c.finish(parts, name)


def build_lock():
    name = "SM_Lock"
    c.reset_scene()
    parts = []

    z0, z1 = LOCK_BODY_Z
    parts.append(c.box(LOCK_BODY, (0, 0, (z0 + z1) / 2), "DIA_Red", bevel=0.02))

    # Shackle: a flat arch, outline in the Y-Z plane swept through X.
    def arc(radius, angles):
        return [(radius * math.cos(a), SHACKLE_ARCH_Z + radius * math.sin(a)) for a in angles]

    angles = [math.pi * i / ARCH_STEPS for i in range(ARCH_STEPS + 1)]       # +Y side -> -Y side
    outline = ([(SHACKLE_OUTER, SHACKLE_FOOT_Z)] + arc(SHACKLE_OUTER, angles)
               + [(-SHACKLE_OUTER, SHACKLE_FOOT_Z), (-SHACKLE_INNER, SHACKLE_FOOT_Z)]
               + arc(SHACKLE_INNER, reversed(angles)) + [(SHACKLE_INNER, SHACKLE_FOOT_Z)])
    parts.append(c.extrude(outline, -SHACKLE_HALF_X, SHACKLE_HALF_X, "X", "DIA_Steel"))

    # Keyhole on both faces: round hole with a slot below.
    face = LOCK_BODY[0] / 2
    for side, axis in ((1, "X"), (-1, "-X")):
        x = side * (face + KEYHOLE_PROUD / 2)
        parts.append(c.cylinder(KEYHOLE_R, KEYHOLE_PROUD, (x, 0, KEYHOLE_Z), axis, FRAME,
                                segments=8, cap_bottom=False))
        parts.append(c.box((KEYHOLE_PROUD, 0.024, 0.07), (x, 0, KEYHOLE_Z - 0.045), FRAME))

    return c.finish(parts, name)


if __name__ == "__main__":
    build_kiosk()
    build_lock()
