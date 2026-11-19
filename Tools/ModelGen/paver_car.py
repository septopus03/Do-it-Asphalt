"""SM_PaverCar - the asphalt paving truck, 4.6 x 3.0 x 2.4 m.

Origin: centre of the footprint, on the ground; it drives towards +X.
A cab-over truck: yellow cab at the front with a sloping glass windscreen, a
side window on each side, the door (seams, handle, step) on the LEFT (-Y) side,
a dark grille between two headlights and a dark bumper whose front face is the
envelope (X = +2.3). Behind the cab a dark flat deck on the chassis carries the
steel tank (a cylinder along X with domed ends, yellow straps, filler hatch) on
two saddles; an exhaust stack stands between cab and tank. Four wheels (tyre,
yellow rim, dark hub) under dark mudguards, yellow lockers between the axles.
At the REAR the steel spreader bar, 3.0 m wide, just above the ground with its
rear face at X = -2.3: hung on two arms from the chassis, braced to the deck
corners and fed by a pipe from the tank. The body without the bar is 2.2 m wide.
Envelope: X +-2.3, Y +-1.5, Z [0, 2.4]. 5 materials.

Run:  blender -b -P Tools/ModelGen/paver_car.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_PaverCar"
BODY, FRAME, TYRE, GLASS, METAL = "DIA_Yellow", "DIA_DarkSteel", "DIA_Rubber", "DIA_Glass", "DIA_Steel"

BODY_HALF_Y = 1.10                   # docs/03: the body without the bar is 2.2 m wide
PANEL_HALF_Y = 1.08                  # cab sides and deck edges; details stand off up to BODY_HALF_Y

# --- wheels -----------------------------------------------------------------
WHEEL_R = 0.50
WHEEL_W = 0.35
WHEEL_SHOULDER = 0.04                # chamfer between tread and side wall
WHEEL_Y = 0.915                      # tyre centre; outer side wall at Y = 1.09
AXLE_XS = (1.35, -1.25)              # front, rear
RIM_R, HUB_R = 0.30, 0.11
RIM_H, HUB_H = 0.004, 0.006          # rim stands off the tyre, hub off the rim: ends at BODY_HALF_Y
GUARD_R = (0.54, 0.60)               # mudguard: half ring around the axle, top touches the deck
GUARD_Y = (0.74, 1.09)
GUARD_STEPS = 8

# --- chassis and deck -------------------------------------------------------
CHASSIS_X = (-2.10, 1.95)
CHASSIS_HALF_Y = 0.45
CHASSIS_Z = (0.60, 1.10)
DECK_X = (-2.10, 0.80)               # ends under the back of the cab
DECK_Z = (1.10, 1.16)
LOCKER_X = (-0.60, 0.60)             # between the mudguards
LOCKER_Z = (0.55, 1.10)

# --- cab --------------------------------------------------------------------
CAB_X = (0.80, 2.20)
CAB_FLOOR_Z = 1.10                   # over the front wheels
SKIRT_X = 1.95                       # front panel in front of the wheels starts here
SKIRT_Z = 0.55
SILL_Z = 1.78                        # bottom of the windows: the top of the 1.8 m figure's head
WINDOW_TOP_Z = 2.24
ROOF_Z = 2.40                        # envelope height
SCREEN_FOOT = (CAB_X[1], 1.70)       # the windscreen face runs from here ...
SCREEN_HEAD = (2.08, 2.30)           # ... up to here, leaning back
# Cab outline in the X-Z plane: back wall, floor, wheel arch step, skirt, nose, windscreen, roof.
CAB_XZ = [(CAB_X[0], CAB_FLOOR_Z), (SKIRT_X, CAB_FLOOR_Z), (SKIRT_X, SKIRT_Z), (CAB_X[1], SKIRT_Z),
          SCREEN_FOOT, SCREEN_HEAD, (1.98, ROOF_Z), (CAB_X[0], ROOF_Z)]
GLASS_STANDOUT = 0.012               # how far the glass stands off the cab panels
GLASS_SINK = 0.02                    # and how far it reaches into them
SCREEN_HALF_Y = 0.95
SIDE_WINDOW_X = (1.22, 2.00)
DOOR_X = (1.14, 2.08)                # door seams on the left side
DOOR_Z0 = 1.16
SEAM_W = 0.02
BUMPER_X = (2.14, 2.30)              # front face = envelope
BUMPER_Z = (0.34, 0.62)
GRILLE_HALF_Y, GRILLE_Z = 0.55, (0.68, 1.22)
LIGHT_Y, LIGHT_Z = 0.82, 0.95

# --- tank -------------------------------------------------------------------
TANK_R = 0.56
TANK_Z = 1.76                        # axis; the tank clears the deck by 4 cm
TANK_X = (-1.95, 0.65)
TANK_DOME = 0.20                     # length of each domed end
STRAP_XS = (-1.35, 0.05)             # straps sit over the saddles
SADDLE_HALF_Y = (0.50, 0.34)         # at the deck, at the top
SADDLE_TOP_Z = 1.52
HATCH_X = -0.65
STACK_XY = (0.73, 0.92)              # exhaust stack between cab and tank
STACK_R, STACK_TOP_Z = 0.05, 2.34

# --- spreader bar (rear) ----------------------------------------------------
BAR_X = (-2.30, -2.18)               # rear face = envelope, the X the game expects
BAR_HALF_Y = 1.50                    # 3.0 m wide: the envelope width
BAR_Z = (0.06, 0.18)                 # just above the ground
NOZZLE_COUNT = 11
NOZZLE_TIP_Z = 0.02
ARM_Y = 0.35
BRACE_Y = (1.30, 1.00)               # on the bar, at the deck corner
FEED_X, FEED_Z = -2.22, 1.50         # the feed pipe leaves the tank's rear dome at this height


def half_ring(axle_x):
    """Mudguard outline around a wheel in the X-Z plane (front end -> over the top ->
    rear end, then back along the inside), as extrude()'s (z, x) pairs for axis 'Y'."""
    pts = []
    for radius, order in ((GUARD_R[1], range(GUARD_STEPS + 1)),
                          (GUARD_R[0], range(GUARD_STEPS, -1, -1))):
        for k in order:
            a = math.pi * k / GUARD_STEPS
            pts.append((WHEEL_R + radius * math.sin(a), axle_x + radius * math.cos(a)))
    return pts


def on_screen(z):
    """X of the windscreen face of the cab at height z."""
    t = (z - SCREEN_FOOT[1]) / (SCREEN_HEAD[1] - SCREEN_FOOT[1])
    return SCREEN_FOOT[0] + t * (SCREEN_HEAD[0] - SCREEN_FOOT[0])


c.reset_scene()
parts = []

# Wheels, mudguards and axles.
half_w = WHEEL_W / 2
tyre = [(RIM_R, -half_w), (WHEEL_R - WHEEL_SHOULDER, -half_w), (WHEEL_R, -half_w + WHEEL_SHOULDER),
        (WHEEL_R, half_w - WHEEL_SHOULDER), (WHEEL_R - WHEEL_SHOULDER, half_w), (RIM_R, half_w)]
for axle_x in AXLE_XS:
    for side in (1, -1):
        outward = "Y" if side > 0 else "-Y"
        face = side * (WHEEL_Y + half_w)
        parts.append(c.lathe(tyre, (axle_x, side * WHEEL_Y, WHEEL_R), "Y", TYRE, segments=20))
        parts.append(c.cylinder(RIM_R, RIM_H, (axle_x, face + side * RIM_H / 2, WHEEL_R), outward, BODY,
                                segments=10, cap_bottom=False))
        parts.append(c.cylinder(HUB_R, HUB_H, (axle_x, face + side * (RIM_H + HUB_H / 2), WHEEL_R),
                                outward, FRAME, segments=10, cap_bottom=False))
        parts.append(c.extrude(half_ring(axle_x), side * GUARD_Y[0], side * GUARD_Y[1], "Y", FRAME))
    parts.append(c.cylinder(0.07, 2 * (WHEEL_Y - half_w), (axle_x, 0, WHEEL_R), "Y", FRAME, segments=8,
                            cap_bottom=False, cap_top=False))
    parts.append(c.box((0.22, 2 * CHASSIS_HALF_Y, 0.18), (axle_x, 0, CHASSIS_Z[0] - 0.07), FRAME))  # springs

# Chassis, deck and the lockers between the axles.
parts.append(c.box((CHASSIS_X[1] - CHASSIS_X[0], 2 * CHASSIS_HALF_Y, CHASSIS_Z[1] - CHASSIS_Z[0]),
                   ((CHASSIS_X[0] + CHASSIS_X[1]) / 2, 0, (CHASSIS_Z[0] + CHASSIS_Z[1]) / 2), FRAME))
parts.append(c.box((DECK_X[1] - DECK_X[0], 2 * PANEL_HALF_Y, DECK_Z[1] - DECK_Z[0]),
                   ((DECK_X[0] + DECK_X[1]) / 2, 0, (DECK_Z[0] + DECK_Z[1]) / 2), FRAME))
for side in (1, -1):
    parts.append(c.box((LOCKER_X[1] - LOCKER_X[0], PANEL_HALF_Y - CHASSIS_HALF_Y,
                        LOCKER_Z[1] - LOCKER_Z[0]),
                       ((LOCKER_X[0] + LOCKER_X[1]) / 2, side * (PANEL_HALF_Y + CHASSIS_HALF_Y) / 2,
                        (LOCKER_Z[0] + LOCKER_Z[1]) / 2), BODY, bevel=0.03))
    parts.append(c.box((0.30, 0.02, 0.04),                                           # locker handle
                       (0.0, side * (PANEL_HALF_Y + 0.01), LOCKER_Z[1] - 0.14), FRAME))

# Cab: body, windscreen, side windows, grille, headlights, bumper.
parts.append(c.extrude([(z, x) for x, z in CAB_XZ], -PANEL_HALF_Y, PANEL_HALF_Y, "Y", BODY))
parts.append(c.extrude([(z, x) for x, z in (
    (on_screen(SILL_Z) - GLASS_SINK, SILL_Z), (on_screen(SILL_Z) + GLASS_STANDOUT, SILL_Z),
    (on_screen(WINDOW_TOP_Z) + GLASS_STANDOUT, WINDOW_TOP_Z),
    (on_screen(WINDOW_TOP_Z) - GLASS_SINK, WINDOW_TOP_Z))], -SCREEN_HALF_Y, SCREEN_HALF_Y, "Y", GLASS))
for side in (1, -1):
    parts.append(c.box((SIDE_WINDOW_X[1] - SIDE_WINDOW_X[0], GLASS_SINK + GLASS_STANDOUT, WINDOW_TOP_Z - SILL_Z),
                       ((SIDE_WINDOW_X[0] + SIDE_WINDOW_X[1]) / 2,
                        side * (PANEL_HALF_Y + (GLASS_STANDOUT - GLASS_SINK) / 2),
                        (SILL_Z + WINDOW_TOP_Z) / 2), GLASS))
    parts.append(c.box((0.03, 0.30, 0.16), (CAB_X[1], side * LIGHT_Y, LIGHT_Z), GLASS))
parts.append(c.box((0.03, 2 * GRILLE_HALF_Y, GRILLE_Z[1] - GRILLE_Z[0]),
                   (CAB_X[1], 0, (GRILLE_Z[0] + GRILLE_Z[1]) / 2), FRAME))
parts.append(c.box((BUMPER_X[1] - BUMPER_X[0], 2 * BODY_HALF_Y, BUMPER_Z[1] - BUMPER_Z[0]),
                   ((BUMPER_X[0] + BUMPER_X[1]) / 2, 0, (BUMPER_Z[0] + BUMPER_Z[1]) / 2), FRAME,
                   bevel=0.04))

# Door on the left (-Y) side: seams, handle and a step in front of the wheel.
door_zm = (DOOR_Z0 + ROOF_Z - 0.08) / 2
for x in DOOR_X:
    parts.append(c.box((SEAM_W, SEAM_W, ROOF_Z - 0.08 - DOOR_Z0), (x, -PANEL_HALF_Y, door_zm), FRAME))
for z in (DOOR_Z0, ROOF_Z - 0.08):
    parts.append(c.box((DOOR_X[1] - DOOR_X[0] + SEAM_W, SEAM_W, SEAM_W),
                       ((DOOR_X[0] + DOOR_X[1]) / 2, -PANEL_HALF_Y, z), FRAME))
parts.append(c.box((0.16, 0.04, 0.05), (DOOR_X[0] + 0.16, -PANEL_HALF_Y, SILL_Z - 0.14), FRAME))
parts.append(c.box((CAB_X[1] - SKIRT_X - 0.04, 0.04, 0.05),
                   ((SKIRT_X + CAB_X[1]) / 2, -PANEL_HALF_Y, SKIRT_Z + 0.20), FRAME))

# Tank on two saddles: domed ends, yellow straps, filler hatch; exhaust stack.
tank_mid = (TANK_X[0] + TANK_X[1]) / 2
tank_half = (TANK_X[1] - TANK_X[0]) / 2
parts.append(c.lathe([(0.0, -tank_half), (0.40, -tank_half + 0.06), (TANK_R, -tank_half + TANK_DOME),
                      (TANK_R, tank_half - TANK_DOME), (0.40, tank_half - 0.06), (0.0, tank_half)],
                     (tank_mid, 0, TANK_Z), "X", METAL, segments=20))
for x in STRAP_XS:
    parts.append(c.ring(TANK_R - 0.02, TANK_R + 0.02, -0.06, 0.06, (x, 0, TANK_Z), "X", BODY, 20))
    parts.append(c.extrude([(-SADDLE_HALF_Y[0], DECK_Z[1]), (SADDLE_HALF_Y[0], DECK_Z[1]),
                            (SADDLE_HALF_Y[1], SADDLE_TOP_Z), (-SADDLE_HALF_Y[1], SADDLE_TOP_Z)],
                           x - 0.09, x + 0.09, "X", FRAME))
parts.append(c.cylinder(0.20, 0.08, (HATCH_X, 0, TANK_Z + TANK_R + 0.02), "Z", METAL, segments=12,
                        cap_bottom=False, cap_top=False))
parts.append(c.cylinder(0.23, 0.03, (HATCH_X, 0, TANK_Z + TANK_R + 0.065), "Z", FRAME, segments=12))
sx, sy = STACK_XY
parts.append(c.cylinder(STACK_R, STACK_TOP_Z - DECK_Z[1], (sx, sy, (DECK_Z[1] + STACK_TOP_Z) / 2), "Z",
                        FRAME, segments=8, cap_bottom=False))
parts.append(c.box((0.14, 0.14, 0.30), (sx, sy, DECK_Z[1] + 0.15), FRAME))           # silencer

# Rear spreader bar: bar with nozzles, two arms, braces to the deck corners, feed pipe.
bar_xm = (BAR_X[0] + BAR_X[1]) / 2
bar_zm = (BAR_Z[0] + BAR_Z[1]) / 2
parts.append(c.box((BAR_X[1] - BAR_X[0], 2 * BAR_HALF_Y, BAR_Z[1] - BAR_Z[0]), (bar_xm, 0, bar_zm), METAL))
for i in range(NOZZLE_COUNT):
    y = (BAR_HALF_Y - 0.10) * (2.0 * i / (NOZZLE_COUNT - 1) - 1.0)
    parts.append(c.cone(0.025, 0.045, BAR_Z[0] - NOZZLE_TIP_Z, (bar_xm, y, (BAR_Z[0] + NOZZLE_TIP_Z) / 2),
                        "-Z", TYRE, segments=6, cap_bottom=False))
for side in (1, -1):
    parts.append(c.bar((CHASSIS_X[0] + 0.06, side * ARM_Y, CHASSIS_Z[0] + 0.15),
                       (bar_xm, side * ARM_Y, bar_zm), 0.10, 0.10, FRAME))
    parts.append(c.bar((bar_xm, side * BRACE_Y[0], bar_zm),
                       (DECK_X[0] + 0.05, side * BRACE_Y[1], DECK_Z[0] + 0.03), 0.06, 0.06, FRAME))
parts.append(c.beam((TANK_X[0] + 0.15, 0, FEED_Z), (FEED_X, 0, FEED_Z), 0.05, METAL, segments=8,
                    cap_start=False))
parts.append(c.beam((FEED_X, 0, FEED_Z + 0.05), (FEED_X, 0, bar_zm), 0.05, METAL, segments=8, cap_end=False))

c.finish(parts, NAME)
