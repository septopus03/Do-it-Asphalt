"""SM_PaverCar, SM_PaverCar_T2, SM_PaverCar_T3, SM_PaverCar_T4 - the asphalt paving
truck in four tiers, each one bigger and heavier than the one before.

  name             L x W x H (m)      body W   bar     axles  tank(s)
  SM_PaverCar      4.6 x 3.0 x 2.4    2.2      3.0 m   2      1, r 0.56
  SM_PaverCar_T2   5.6 x 3.6 x 2.7    2.5      3.6 m   2      1, r 0.64
  SM_PaverCar_T3   6.8 x 4.2 x 3.0    2.8      4.2 m   3      1, r 0.72
  SM_PaverCar_T4   8.2 x 5.0 x 3.4    3.1      5.0 m   4      2, r 0.68 side by side

Origin: centre of the footprint, on the ground; it drives towards +X.
A cab-over truck: yellow cab at the front with a sloping glass windscreen, a
side window on each side, the door (seams, handle, steps) on the LEFT (-Y) side,
a dark grille between the headlights and a dark bumper. Behind the cab a dark
flat deck on the chassis carries the steel tank (a cylinder along X with domed
ends, yellow straps, filler hatch) on two saddles; an exhaust stack stands
between cab and tank. Wheels (tyre, yellow rim, dark hub) under dark mudguards,
yellow lockers between the axles. At the REAR the steel spreader bar, as wide as
the envelope, just above the ground with its rear face at X = -L/2: hung on two
arms from the chassis, braced to the deck corners and fed by a pipe from each tank.

What the tiers add on top of the size:
  T2  sun visor, mirrors, roof light bar with two beacons, a second cab step
  T3  + a third axle (rear tandem), a second exhaust stack, a bull bar, stacked
      headlights, a tank ladder, hydraulic rams and yellow end plates on the bar
  T4  + a fourth axle (two front, two rear), two tanks side by side with a
      catwalk between them, a third cab step

The front face of the bumper (the bull bar on T3 and T4) and the rear face of the
bar are the X envelope, the bar ends the Y envelope, the cab roof (the roof
lights from T2 up) the Z envelope. Envelopes: see TIERS and manifest.json.
SM_PaverCar is the tier-1 truck the game uses today; its numbers must not change.
5 materials each.

Run:  blender -b -P Tools/ModelGen/paver_car.py
"""
import math
import os
import sys
from types import SimpleNamespace

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

BODY, FRAME, TYRE, GLASS, METAL = "DIA_Yellow", "DIA_DarkSteel", "DIA_Rubber", "DIA_Glass", "DIA_Steel"

# --- the same on every tier -------------------------------------------------
WHEEL_SHOULDER = 0.04                # chamfer between tread and side wall
RIM_H, HUB_H = 0.004, 0.006          # rim stands off the tyre, hub off the rim: ends at the body width
GUARD_STEPS = 8
GLASS_STANDOUT = 0.012               # how far the glass stands off the cab panels
GLASS_SINK = 0.02                    # and how far it reaches into them
SEAM_W = 0.02
BAR_Z0 = 0.06                        # the spreader bar hangs just above the ground
NOZZLE_PITCH = 0.28
NOZZLE_TIP_Z = 0.02
TIER1_CAB_H = 1.30                   # cab height (floor to roof) and tank radius of the tier-1 truck:
TIER1_TANK_R = 0.56                  # the taller cabs and fatter tanks are scaled from these


def tier(name, half_x, body_half_y, bar_half_y, height, wheel_r, wheel_w, axle_xs, cab_len, cab_roof_z,
         tank_r, tank_ys=(0.0,), chassis_half_y=0.45, stack_r=0.05, stack_sides=(1,), stack_top_z=None,
         bar_depth=0.12, bar_top_z=0.18, steps=1, visor=False, mirrors=False, roof_lights=0,
         bull_bar=False, stacked_lights=False, ladder=False, rams=False, end_plates=False, catwalk=False):
    """One truck. `half_x`, `bar_half_y`, `height` are the envelope; `axle_xs` runs front to rear.
    `stack_sides`: +1 = right (+Y), -1 = left. `cab_roof_z` is below `height` when roof lights sit on it."""
    return SimpleNamespace(**locals())


TIERS = (
    tier("SM_PaverCar", 2.30, 1.10, 1.50, 2.40, 0.50, 0.35, (1.35, -1.25), 1.40, 2.40, 0.56),
    tier("SM_PaverCar_T2", 2.80, 1.25, 1.80, 2.70, 0.56, 0.40, (1.79, -1.65), 1.55, 2.62, 0.64,
         chassis_half_y=0.50, stack_r=0.055, steps=2, visor=True, mirrors=True, roof_lights=2),
    tier("SM_PaverCar_T3", 3.40, 1.40, 2.10, 3.00, 0.62, 0.44, (2.33, -0.72, -2.20), 1.70, 2.90, 0.72,
         chassis_half_y=0.55, stack_r=0.06, stack_sides=(1, -1), stack_top_z=2.98, bar_depth=0.16,
         bar_top_z=0.22, steps=2, visor=True, mirrors=True, roof_lights=4, bull_bar=True,
         stacked_lights=True, ladder=True, rams=True, end_plates=True),
    tier("SM_PaverCar_T4", 4.10, 1.55, 2.50, 3.40, 0.70, 0.50, (2.95, 1.31, -1.21, -2.85), 1.90, 3.28, 0.68,
         tank_ys=(0.74, -0.74), chassis_half_y=0.62, stack_r=0.065, stack_sides=(1, -1), stack_top_z=3.38,
         bar_depth=0.20, bar_top_z=0.26, steps=3, visor=True, mirrors=True, roof_lights=6, bull_bar=True,
         stacked_lights=True, ladder=True, rams=True, end_plates=True, catwalk=True),
)


def half_ring(axle_x, wheel_r, guard_r):
    """Mudguard outline around a wheel in the X-Z plane (front end -> over the top ->
    rear end, then back along the inside), as extrude()'s (z, x) pairs for axis 'Y'."""
    pts = []
    for radius, order in ((guard_r[1], range(GUARD_STEPS + 1)),
                          (guard_r[0], range(GUARD_STEPS, -1, -1))):
        for k in order:
            a = math.pi * k / GUARD_STEPS
            pts.append((wheel_r + radius * math.sin(a), axle_x + radius * math.cos(a)))
    return pts


def build(t):
    c.reset_scene()
    parts = []

    # --- measures derived from the tier ---------------------------------------
    panel_half_y = t.body_half_y - 0.02              # cab sides and deck edges; details stand off up to the body width
    half_w = t.wheel_w / 2
    wheel_y = t.body_half_y - 0.01 - half_w          # tyre centre; rim and hub end at the body width
    rim_r, hub_r = 0.6 * t.wheel_r, 0.22 * t.wheel_r
    guard_r = (t.wheel_r + 0.04, t.wheel_r + 0.10)   # mudguard: half ring around the axle, top touches the deck
    guard_y = (wheel_y - half_w, t.body_half_y - 0.01)
    deck_z0 = 2 * t.wheel_r + 0.10                   # top of the mudguards = top of the chassis = cab floor
    chassis_x = (-t.half_x + 0.20, t.half_x - 0.35)
    chassis_z = (t.wheel_r + 0.10, deck_z0)
    cab_x = (t.half_x - 0.10 - t.cab_len, t.half_x - 0.10)
    deck_x = (chassis_x[0], cab_x[0])                # ends under the back of the cab
    deck_z = (deck_z0, deck_z0 + 0.06)
    front_x = t.half_x - (0.08 if t.bull_bar else 0.0)   # front face of the bumper; the bull bar stands in front of it

    cab_h = t.cab_roof_z - deck_z0
    grow = cab_h - TIER1_CAB_H                       # how much taller the cab is than the tier-1 one
    skirt_x = cab_x[1] - 0.25                        # front panel in front of the wheels starts here
    skirt_z = t.wheel_r + 0.05
    sill_z = deck_z0 + 0.68 + 0.5 * grow             # bottom of the windows (tier 1: the top of the 1.8 m figure's head)
    window_top_z = t.cab_roof_z - 0.16
    screen_foot = (cab_x[1], sill_z - 0.08)          # the windscreen face runs from here ...
    screen_head = (cab_x[1] - 0.12, t.cab_roof_z - 0.10)   # ... up to here, leaning back
    screen_half_y = panel_half_y - 0.13
    side_window_x = (cab_x[0] + 0.42, cab_x[1] - 0.20)
    door_x = (cab_x[0] + 0.34, cab_x[1] - 0.12)      # door seams on the left side
    door_z0 = deck_z0 + 0.06
    bumper_x = (cab_x[1] - 0.06, front_x)
    bumper_z = (t.wheel_r - 0.16, t.wheel_r + 0.12 + 0.25 * grow)
    grille_half_y = t.body_half_y / 2
    grille_z = (t.wheel_r + 0.18, deck_z0 + 0.12 + 0.6 * grow)
    light_y, light_z = t.body_half_y - 0.28, deck_z0 - 0.15

    k = t.tank_r / TIER1_TANK_R                      # tank details scale with its radius
    tank_z = deck_z0 + 0.10 + t.tank_r               # axis; the tank clears the deck by 4 cm
    tank_x = (-t.half_x + 0.35, cab_x[0] - 0.15)
    tank_mid = (tank_x[0] + tank_x[1]) / 2
    tank_half = (tank_x[1] - tank_x[0]) / 2
    strap_xs = (tank_x[0] + 0.60, tank_x[1] - 0.60)  # straps sit over the saddles
    stack_x = cab_x[0] - 0.07                        # exhaust stack between cab and tank
    stack_y = t.body_half_y - 0.18
    stack_top_z = t.stack_top_z if t.stack_top_z is not None else t.cab_roof_z - 0.06

    bar_x = (-t.half_x, -t.half_x + t.bar_depth)     # rear face = envelope, the X the game expects
    bar_z = (BAR_Z0, t.bar_top_z)
    bar_xm = (bar_x[0] + bar_x[1]) / 2
    bar_zm = (bar_z[0] + bar_z[1]) / 2
    arm_y = t.chassis_half_y - 0.10
    brace_y = (t.bar_half_y - 0.20, panel_half_y - 0.08)   # on the bar, at the deck corner
    feed_x, feed_z = -t.half_x + 0.08, tank_z - 0.26 * k   # the feed pipe leaves the tank's rear dome at this height

    def on_screen(z):
        """X of the windscreen face of the cab at height z."""
        f = (z - screen_foot[1]) / (screen_head[1] - screen_foot[1])
        return screen_foot[0] + f * (screen_head[0] - screen_foot[0])

    # --- wheels, mudguards and axles ------------------------------------------
    tyre = [(rim_r, -half_w), (t.wheel_r - WHEEL_SHOULDER, -half_w), (t.wheel_r, -half_w + WHEEL_SHOULDER),
            (t.wheel_r, half_w - WHEEL_SHOULDER), (t.wheel_r - WHEEL_SHOULDER, half_w), (rim_r, half_w)]
    for axle_x in t.axle_xs:
        for side in (1, -1):
            outward = "Y" if side > 0 else "-Y"
            face = side * (wheel_y + half_w)
            parts.append(c.lathe(tyre, (axle_x, side * wheel_y, t.wheel_r), "Y", TYRE, segments=20))
            parts.append(c.cylinder(rim_r, RIM_H, (axle_x, face + side * RIM_H / 2, t.wheel_r), outward, BODY,
                                    segments=10, cap_bottom=False))
            parts.append(c.cylinder(hub_r, HUB_H, (axle_x, face + side * (RIM_H + HUB_H / 2), t.wheel_r),
                                    outward, FRAME, segments=10, cap_bottom=False))
            parts.append(c.extrude(half_ring(axle_x, t.wheel_r, guard_r), side * guard_y[0], side * guard_y[1],
                                   "Y", FRAME))
        parts.append(c.cylinder(0.07, 2 * (wheel_y - half_w), (axle_x, 0, t.wheel_r), "Y", FRAME, segments=8,
                                cap_bottom=False, cap_top=False))
        parts.append(c.box((0.22, 2 * t.chassis_half_y, 0.18), (axle_x, 0, chassis_z[0] - 0.07), FRAME))  # springs

    # --- chassis, deck and the lockers between the front and the rear axles ---
    parts.append(c.box((chassis_x[1] - chassis_x[0], 2 * t.chassis_half_y, chassis_z[1] - chassis_z[0]),
                       ((chassis_x[0] + chassis_x[1]) / 2, 0, (chassis_z[0] + chassis_z[1]) / 2), FRAME))
    parts.append(c.box((deck_x[1] - deck_x[0], 2 * panel_half_y, deck_z[1] - deck_z[0]),
                       ((deck_x[0] + deck_x[1]) / 2, 0, (deck_z[0] + deck_z[1]) / 2), FRAME))
    # The widest gap between two neighbouring axles is the one between the front and the rear group.
    gap_front, gap_rear = max(zip(t.axle_xs, t.axle_xs[1:]), key=lambda pair: pair[0] - pair[1])
    locker_x = (gap_rear + guard_r[1] + 0.05, gap_front - guard_r[1] - 0.15)
    locker_z = (t.wheel_r + 0.05, deck_z0)
    locker_xm = (locker_x[0] + locker_x[1]) / 2
    for side in (1, -1):
        parts.append(c.box((locker_x[1] - locker_x[0], panel_half_y - t.chassis_half_y,
                            locker_z[1] - locker_z[0]),
                           (locker_xm, side * (panel_half_y + t.chassis_half_y) / 2,
                            (locker_z[0] + locker_z[1]) / 2), BODY, bevel=0.03))
        parts.append(c.box((0.30, 0.02, 0.04),                                           # locker handle
                           (locker_xm, side * (panel_half_y + 0.01), locker_z[1] - 0.14), FRAME))

    # --- cab: body, windscreen, side windows, grille, headlights, bumper ------
    # Outline in the X-Z plane: back wall, floor, wheel arch step, skirt, nose, windscreen, roof.
    cab_xz = [(cab_x[0], deck_z0), (skirt_x, deck_z0), (skirt_x, skirt_z), (cab_x[1], skirt_z),
              screen_foot, screen_head, (cab_x[1] - 0.22, t.cab_roof_z), (cab_x[0], t.cab_roof_z)]
    parts.append(c.extrude([(z, x) for x, z in cab_xz], -panel_half_y, panel_half_y, "Y", BODY))
    parts.append(c.extrude([(z, x) for x, z in (
        (on_screen(sill_z) - GLASS_SINK, sill_z), (on_screen(sill_z) + GLASS_STANDOUT, sill_z),
        (on_screen(window_top_z) + GLASS_STANDOUT, window_top_z),
        (on_screen(window_top_z) - GLASS_SINK, window_top_z))], -screen_half_y, screen_half_y, "Y", GLASS))
    light_zs = (light_z, light_z + 0.22) if t.stacked_lights else (light_z,)
    for side in (1, -1):
        parts.append(c.box((side_window_x[1] - side_window_x[0], GLASS_SINK + GLASS_STANDOUT, window_top_z - sill_z),
                           ((side_window_x[0] + side_window_x[1]) / 2,
                            side * (panel_half_y + (GLASS_STANDOUT - GLASS_SINK) / 2),
                            (sill_z + window_top_z) / 2), GLASS))
        for z in light_zs:
            parts.append(c.box((0.03, 0.30, 0.16), (cab_x[1], side * light_y, z), GLASS))
    parts.append(c.box((0.03, 2 * grille_half_y, grille_z[1] - grille_z[0]),
                       (cab_x[1], 0, (grille_z[0] + grille_z[1]) / 2), FRAME))
    parts.append(c.box((bumper_x[1] - bumper_x[0], 2 * t.body_half_y, bumper_z[1] - bumper_z[0]),
                       ((bumper_x[0] + bumper_x[1]) / 2, 0, (bumper_z[0] + bumper_z[1]) / 2), FRAME,
                       bevel=0.02 if t.bull_bar else 0.04))   # behind a bull bar the bumper is only 8 cm deep

    # Door on the left (-Y) side: seams, handle and the steps in front of the wheel.
    door_top = t.cab_roof_z - 0.08
    door_zm = (door_z0 + door_top) / 2
    for x in door_x:
        parts.append(c.box((SEAM_W, SEAM_W, door_top - door_z0), (x, -panel_half_y, door_zm), FRAME))
    for z in (door_z0, door_top):
        parts.append(c.box((door_x[1] - door_x[0] + SEAM_W, SEAM_W, SEAM_W),
                           ((door_x[0] + door_x[1]) / 2, -panel_half_y, z), FRAME))
    parts.append(c.box((0.16, 0.04, 0.05), (door_x[0] + 0.16, -panel_half_y, sill_z - 0.14), FRAME))
    step_rise = 0.0 if t.steps == 1 else (door_z0 - 0.12 - (skirt_z + 0.20)) / (t.steps - 1)
    for i in range(t.steps):
        parts.append(c.box((cab_x[1] - skirt_x - 0.04, 0.04, 0.05),
                           ((skirt_x + cab_x[1]) / 2, -panel_half_y, skirt_z + 0.20 + i * step_rise), FRAME))

    # --- tank(s) on two saddles: domed ends, yellow straps, filler hatch -------
    for ty in t.tank_ys:
        parts.append(c.lathe([(0.0, -tank_half), (0.40 * k, -tank_half + 0.06 * k),
                              (t.tank_r, -tank_half + 0.20 * k), (t.tank_r, tank_half - 0.20 * k),
                              (0.40 * k, tank_half - 0.06 * k), (0.0, tank_half)],
                             (tank_mid, ty, tank_z), "X", METAL, segments=20))
        for x in strap_xs:
            parts.append(c.ring(t.tank_r - 0.02, t.tank_r + 0.02, -0.06, 0.06, (x, ty, tank_z), "X", BODY, 20))
            parts.append(c.extrude([(ty - 0.50 * k, deck_z[1]), (ty + 0.50 * k, deck_z[1]),
                                    (ty + 0.34 * k, tank_z - 0.24 * k), (ty - 0.34 * k, tank_z - 0.24 * k)],
                                   x - 0.09, x + 0.09, "X", FRAME))
        parts.append(c.cylinder(0.20, 0.08, (tank_mid, ty, tank_z + t.tank_r + 0.02), "Z", METAL, segments=12,
                                cap_bottom=False, cap_top=False))
        parts.append(c.cylinder(0.23, 0.03, (tank_mid, ty, tank_z + t.tank_r + 0.065), "Z", FRAME, segments=12))
        # Feed pipe from the tank's rear dome down to the bar.
        parts.append(c.beam((tank_x[0] + 0.15, ty, feed_z), (feed_x, ty, feed_z), 0.05, METAL, segments=8,
                            cap_start=False))
        parts.append(c.beam((feed_x, ty, feed_z + 0.05), (feed_x, ty, bar_zm), 0.05, METAL, segments=8,
                            cap_end=False))

    # --- exhaust stack(s) with a silencer ---------------------------------------
    silencer = 0.14 * t.stack_r / 0.05
    for side in t.stack_sides:
        parts.append(c.cylinder(t.stack_r, stack_top_z - deck_z[1],
                                (stack_x, side * stack_y, (deck_z[1] + stack_top_z) / 2), "Z",
                                FRAME, segments=8, cap_bottom=False))
        parts.append(c.box((0.14, silencer, 0.30 * t.stack_r / 0.05),
                           (stack_x, side * stack_y, deck_z[1] + 0.15 * t.stack_r / 0.05), FRAME))

    # --- rear spreader bar: bar with nozzles, two arms, braces to the deck corners
    parts.append(c.box((bar_x[1] - bar_x[0], 2 * t.bar_half_y, bar_z[1] - bar_z[0]), (bar_xm, 0, bar_zm), METAL))
    nozzle_count = int(round(2 * (t.bar_half_y - 0.10) / NOZZLE_PITCH)) + 1
    for i in range(nozzle_count):
        y = (t.bar_half_y - 0.10) * (2.0 * i / (nozzle_count - 1) - 1.0)
        parts.append(c.cone(0.025, 0.045, bar_z[0] - NOZZLE_TIP_Z, (bar_xm, y, (bar_z[0] + NOZZLE_TIP_Z) / 2),
                            "-Z", TYRE, segments=6, cap_bottom=False))
    for side in (1, -1):
        parts.append(c.bar((chassis_x[0] + 0.06, side * arm_y, chassis_z[0] + 0.15),
                           (bar_xm, side * arm_y, bar_zm), 0.10, 0.10, FRAME))
        parts.append(c.bar((bar_xm, side * brace_y[0], bar_zm),
                           (deck_x[0] + 0.05, side * brace_y[1], deck_z[0] + 0.03), 0.06, 0.06, FRAME))
        if t.rams:                                   # hydraulic ram from the deck's rear edge down to the bar
            ram_y = side * (arm_y + 0.30)
            parts.append(c.beam((deck_x[0] + 0.12, ram_y, deck_z[1] + 0.10), (bar_xm, ram_y, bar_z[1]), 0.045,
                                METAL, segments=8))
            parts.append(c.box((0.14, 0.14, 0.12), (deck_x[0] + 0.12, ram_y, deck_z[1] + 0.06), FRAME))
        if t.end_plates:                             # yellow plate on each end of the bar
            parts.append(c.box((t.bar_depth, 0.02, bar_z[1] - bar_z[0] + 0.06),
                               (bar_xm, side * (t.bar_half_y - 0.01), bar_zm + 0.03), BODY))

    # --- what the heavier tiers add ---------------------------------------------
    if t.visor:                                      # sun visor over the windscreen
        parts.append(c.box((0.18, 2 * screen_half_y + 0.10, 0.04),
                           (on_screen(window_top_z) + 0.07, 0, window_top_z + 0.04), FRAME))
    if t.mirrors:                                    # mirror on an arm at each front corner of the cab
        mirror_z = sill_z + 0.5 * (window_top_z - sill_z)
        for side in (1, -1):
            parts.append(c.box((0.03, 0.16, 0.03), (cab_x[1] - 0.16, side * (panel_half_y + 0.07), mirror_z + 0.10),
                               FRAME))
            parts.append(c.box((0.04, 0.12, 0.30), (cab_x[1] - 0.16, side * (panel_half_y + 0.16), mirror_z), FRAME))
    if t.roof_lights:                                # light bar on the roof: beacons on the ends, lamps between
        bar_cx = cab_x[1] - 0.50
        rail_half = panel_half_y - 0.18
        top = t.height - t.cab_roof_z
        parts.append(c.box((0.14, 2 * rail_half, 0.03), (bar_cx, 0, t.cab_roof_z + 0.015), FRAME))
        for i in range(t.roof_lights):
            y = (rail_half - 0.10) * (2.0 * i / (t.roof_lights - 1) - 1.0)
            if i in (0, t.roof_lights - 1):
                parts.append(c.cylinder(0.07, top - 0.03, (bar_cx, y, t.cab_roof_z + 0.03 + (top - 0.03) / 2),
                                        "Z", BODY, segments=8, cap_bottom=False))
            else:
                parts.append(c.box((0.08, 0.20, top - 0.05), (bar_cx, y, t.cab_roof_z + 0.03 + (top - 0.05) / 2),
                                   GLASS))
    if t.bull_bar:                                   # steel tubes in front of the bumper and the grille
        tube_r = 0.04
        tube_x = t.half_x - tube_r                   # front of the tubes = envelope
        post_y = grille_half_y + 0.12
        bull_z = (bumper_z[0] + 0.04, grille_z[1] - 0.06)
        for side in (1, -1):
            parts.append(c.beam((tube_x, side * post_y, bull_z[0]), (tube_x, side * post_y, bull_z[1]), tube_r,
                                METAL, segments=8))
            # stay from the post back to the cab front
            parts.append(c.box((tube_x - cab_x[1], 0.05, 0.05),
                               ((tube_x + cab_x[1]) / 2, side * post_y, bull_z[1] - 0.10), FRAME))
        for z in (bull_z[1], (bull_z[0] + bull_z[1]) / 2 + 0.05):
            parts.append(c.beam((tube_x, -post_y - 0.10, z), (tube_x, post_y + 0.10, z), tube_r, METAL, segments=8))
    if t.ladder:                                     # ladder up the right side of the (right) tank
        rail_y = max(t.tank_ys) + t.tank_r + 0.05
        ladder_x = tank_mid + 0.55
        ladder_top = tank_z + 0.55 * t.tank_r
        for dx in (-0.20, 0.20):
            parts.append(c.box((0.04, 0.04, ladder_top - deck_z[1]),
                               (ladder_x + dx, rail_y, (deck_z[1] + ladder_top) / 2), FRAME))
        rungs = int((ladder_top - deck_z[1] - 0.10) / 0.28)
        for i in range(rungs):
            parts.append(c.box((0.36, 0.03, 0.03), (ladder_x, rail_y, deck_z[1] + 0.22 + i * 0.28), FRAME))
    if t.catwalk:                                    # walkway on top between the two tanks, with a hand rail
        walk_z = tank_z + 0.80 * t.tank_r
        walk_x = (tank_x[0] + 0.45, tank_x[1] - 0.45)
        walk_xm = (walk_x[0] + walk_x[1]) / 2
        parts.append(c.box((walk_x[1] - walk_x[0], 0.56, 0.04), (walk_xm, 0, walk_z), FRAME))
        rail_z = tank_z + t.tank_r + 0.16           # level with the top of the filler hatches' covers plus a hand
        for x in (walk_x[0] + 0.03, walk_xm, walk_x[1] - 0.03):
            parts.append(c.box((0.03, 0.03, rail_z - walk_z), (x, 0, (walk_z + rail_z) / 2), FRAME))
        parts.append(c.box((walk_x[1] - walk_x[0], 0.03, 0.03), (walk_xm, 0, rail_z), BODY))

    c.finish(parts, t.name)


for _tier in TIERS:
    build(_tier)
