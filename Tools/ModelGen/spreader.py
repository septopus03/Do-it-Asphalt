"""SM_Spreader_020 ... SM_Spreader_200 - the hand-pushed asphalt spreader, 7 widths.

One parametric build(width_m): a yellow spray bar of width W on the Y axis at
X = 0 just above the ground, a wheel (radius 6 cm) at each end, black nozzles
along the front, and a fixed-size handle rising backwards from the middle of the
bar to the grip point (-0.45, 0, 0.95) with a cross grip and an orange valve
lever. A short hose leaves the back of the bar towards the player. From 1.0 m up
the bar is split in the middle around a third, supporting wheel.

Envelope: X [-0.50, 0.08], Y +-(W/2 + 0.05), Z [0, 1.0]. Origin: on the ground
under the middle of the bar. 5 materials, at most 600 triangles.

Run:  blender -b -P Tools/ModelGen/spreader.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

WIDTHS = (0.2, 0.3, 0.5, 0.8, 1.0, 1.5, 2.0)
CENTRE_WHEEL_FROM = 1.0

WHEEL_R = 0.06
WHEEL_T = 0.045           # tyre thickness; the hub cap adds 5 mm -> W/2 + 0.05
BAR_X = (-0.04, 0.03)     # bar section, back - front
BAR_Z = (0.045, 0.105)    # just above the ground, inside the wheel outline
GAP = 0.035               # half width of the slot for the centre wheel
GRIP = (-0.45, 0.0, 0.95)  # where the hands hold it (docs/03 chapter 4.1)
JOINT = (-0.13, 0.0, 0.28)  # the fork meets the shaft here


def bar_piece(y0, y1):
    """Yellow bar section between y0 and y1: box with chamfered top edges."""
    (x0, x1), (z0, z1) = BAR_X, BAR_Z
    ch = 0.018
    outline_zx = [(z0, x0), (z0, x1), (z1 - ch, x1), (z1, x1 - ch), (z1, x0 + ch), (z1 - ch, x0)]
    return c.extrude(outline_zx, y0, y1, "Y", "DIA_Yellow")


def wheel(y_inner, outward, hub=True):
    """Tyre whose inner face is at y_inner, growing towards `outward` (+1 / -1)."""
    y_mid = y_inner + outward * WHEEL_T / 2
    parts = [c.cylinder(WHEEL_R, WHEEL_T, (0, y_mid, WHEEL_R), "Y", "DIA_Rubber", segments=12)]
    if hub:
        y_hub = y_inner + outward * (WHEEL_T + 0.0025)
        parts.append(c.cylinder(0.028, 0.005, (0, y_hub, WHEEL_R), "Y" if outward > 0 else "-Y",
                                "DIA_Steel", segments=6, cap_bottom=False))
    return parts


def build(width_m):
    name = "SM_Spreader_%03d" % round(width_m * 100)
    half = width_m / 2.0
    centre_wheel = width_m >= CENTRE_WHEEL_FROM - 1e-9
    c.reset_scene()
    parts = []

    # Bar and wheels.
    if centre_wheel:
        parts += [bar_piece(-half, -GAP), bar_piece(GAP, half)]
        parts.append(c.cylinder(0.016, 2 * GAP, (0, 0, WHEEL_R), "Y", "DIA_Steel", segments=6,
                                cap_bottom=False, cap_top=False))          # axle
        parts.append(c.cylinder(WHEEL_R, 0.04, (0, 0, WHEEL_R), "Y", "DIA_Rubber", segments=12))
    else:
        parts.append(bar_piece(-half, half))
    parts += wheel(half, +1) + wheel(-half, -1)

    # Nozzles: flared square spouts along the front edge, pointing forward and down.
    count = max(2, min(12, round(width_m / 0.1)))
    lo = (GAP + 0.045) if centre_wheel else None
    ys = []
    if centre_wheel:
        per_side = count // 2
        for i in range(per_side):
            y = lo + (half - 0.045 - lo) * i / (per_side - 1)
            ys += [y, -y]
    else:
        ys = [(-half + 0.045) + (width_m - 0.09) * i / (count - 1) for i in range(count)]
    for y in ys:
        parts.append(c.beam((0.015, y, 0.078), (0.068, y, 0.038), 0.014, "DIA_Asphalt",
                            segments=4, radius_end=0.021, cap_start=False))

    # Handle: a fork from the bar to the joint, the shaft, the cross grip.
    fork_y = min(0.09, half - 0.03)
    for side in (1, -1):
        parts.append(c.beam((-0.015, side * fork_y, BAR_Z[1] - 0.01), JOINT, 0.013, "DIA_Steel",
                            segments=6, cap_start=False, cap_end=False))
    parts.append(c.box((0.06, 0.06, 0.06), JOINT, "DIA_Yellow"))
    parts.append(c.beam(JOINT, GRIP, 0.017, "DIA_Steel", segments=6,
                        cap_start=False, cap_end=False))
    grip_half = min(0.18, half + 0.04)
    gx, _, gz = GRIP
    parts.append(c.cylinder(0.015, 2 * grip_half - 0.02, GRIP, "Y", "DIA_Steel", segments=6,
                            cap_bottom=False, cap_top=False))
    grip_len = 0.09
    for side in (1, -1):
        parts.append(c.cylinder(0.024, grip_len, (gx, side * (grip_half - grip_len / 2), gz),
                                "Y", "DIA_Rubber", segments=8))
    # Valve lever on top of the grip: orange, the highest and rearmost point.
    parts.append(c.box((0.06, 0.036, 0.03), (-0.47, 0, 0.985), "DIA_Orange"))

    # Hose: from the back of the bar, curving up towards the player; steel coupling.
    hy = 0.045 if fork_y > 0.075 else 0.0   # the 0.2 m bar is too short for an offset
    path = [(BAR_X[0] + 0.01, hy, 0.08), (-0.10, hy, 0.10), (-0.175, hy * 0.7, 0.24),
            (-0.235, hy * 0.6 + 0.01, 0.43)]
    for a, b in zip(path, path[1:]):
        parts.append(c.beam(a, b, 0.016, "DIA_Rubber", segments=6, cap_start=False, cap_end=False))
    a, b = path[-2], path[-1]
    d = [b[i] - a[i] for i in range(3)]
    n = sum(x * x for x in d) ** 0.5
    tip = [b[i] + d[i] / n * 0.035 for i in range(3)]
    back = [b[i] - d[i] / n * 0.005 for i in range(3)]
    parts.append(c.beam(back, tip, 0.022, "DIA_Steel", segments=6))

    c.assert_outward(parts)
    obj = c.join(parts, name)
    c.export(obj, name)
    c.render_previews(obj, name)
    return obj


if __name__ == "__main__":
    for w in WIDTHS:
        build(w)
