"""SM_Backpack_S / _M / _L / _XL - the asphalt tank the player carries on the back.

Origin: centre of the face that touches the player's back (the plane X = 0);
everything lies in -X. Envelope: X [-depth, 0], Y and Z symmetric about the origin.

All four share one design, scaled with the height: a light steel back plate with two
shoulder straps and a waist belt on its front, an oval yellow tank with a steel
filler lid, black bands around the tank, a sight glass with a scale (a DIA_Glass
level tube with dark rings) standing off the rear-right of the tank - visible from
behind, from the right and from the front-right - and a hose outlet at the bottom.
  S, M  one tank
  L     the tank plus a smaller steel canister on its left (-Y), joined by a pipe
  XL    two big tanks side by side in a DIA_DarkSteel tube frame, joined by a manifold

5 materials, at most 800 triangles each.

Run:  blender -b -P Tools/ModelGen/backpack.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

# name -> (depth X, width Y, height Z) in metres, docs/03-3d-modely.md chapter 4.1
SIZES = {
    "S": (0.16, 0.26, 0.32),
    "M": (0.22, 0.32, 0.42),
    "L": (0.28, 0.38, 0.52),
    "XL": (0.34, 0.44, 0.62),
}

PLATE_FRONT = -0.006   # the straps fill the last millimetres up to X = 0
PLATE_BACK = -0.020
BAND_OUT = 1.035       # bands stand 3.5 % proud of the tank
PLATE_WIDTH = {"S": 0.70, "M": 0.70, "L": 0.84, "XL": 1.0}   # share of the full width
GAUGE_ANGLE = 45.0     # sight glass: degrees from +Y (right) towards -X (rear)


def tank(xc, yc, rx, ry, zb, zs, s, mat="DIA_Yellow", segments=12):
    """Oval vessel: flat bottom at zb, shoulder at zs, short neck above it."""
    part = c.lathe([(0.80, zb), (1.0, zb + 0.03 * s), (1.0, zs), (0.80, zs + 0.035 * s),
                    (0.42, zs + 0.055 * s)], (0, 0, 0), "Z", mat, segments=segments)
    c.scale(part, (rx, ry, 1.0))
    return c.translate(part, (xc, yc, 0))


def band(xc, yc, rx, ry, z0, z1, segments=12):
    """Black strap around a tank between heights z0 and z1."""
    e = 0.2 * (z1 - z0)
    part = c.lathe([(1.0, z0), (BAND_OUT, z0 + e), (BAND_OUT, z1 - e), (1.0, z1)], (0, 0, 0), "Z",
                   "DIA_Rubber", segments=segments, cap_start=False, cap_end=False)
    c.scale(part, (rx, ry, 1.0))
    return c.translate(part, (xc, yc, 0))


def lid(xc, yc, radius, z0, z1, s, mat="DIA_Steel"):
    """Filler cap: a neck with a wider screw lid on top, up to z1."""
    return c.lathe([(0.6 * radius, z0), (0.6 * radius, z0 + 0.012 * s), (radius, z0 + 0.012 * s),
                    (radius, z1)], (xc, yc, 0), "Z", mat, segments=8, cap_start=False)


def gauge_reach(rx, ry, s):
    """How far the sight glass of a tank with radii rx, ry reaches from the tank's
    axis: (towards -X, towards the side). Used to keep it inside the envelope."""
    t = math.radians(GAUGE_ANGLE)
    nx, ny = math.sin(t) / rx, math.cos(t) / ry
    n = math.hypot(nx, ny)
    off, r = 0.020 * s, 0.014 * s * 1.25
    return (rx * math.sin(t) + nx / n * off + r,
            ry * math.cos(t) + ny / n * off + r * math.cos(math.radians(30)))


def fit_tank(front, rear, y_in, y_out, s):
    """Radii and centre (rx, ry, xc, yc) of the largest oval tank, bands and sight
    glass included, between X front..rear and Y y_in..y_out."""
    rx = (front - rear) / (1.0 + BAND_OUT)
    ry = (y_out - y_in) / (2.0 * BAND_OUT)
    for _ in range(40):
        gx, gy = gauge_reach(rx, ry, s)
        rx *= (front - rear) / (rx + max(rx * BAND_OUT, gx))
        ry *= (y_out - y_in) / (ry * BAND_OUT + max(ry * BAND_OUT, gy))
    return rx, ry, front - rx, y_in + ry * BAND_OUT


def gauge(xc, yc, rx, ry, z0, z1, s, side=1):
    """Sight glass: a glass level tube with scale rings, standing off the oval tank
    at GAUGE_ANGLE on two steel fittings, so it shows past the tank's outline from
    the rear, the side and the front quarter."""
    t = math.radians(GAUGE_ANGLE)
    px, py = xc - rx * math.sin(t), yc + side * ry * math.cos(t)      # point on the tank
    nx, ny = -math.sin(t) / rx, side * math.cos(t) / ry               # outward normal there
    n = math.hypot(nx, ny)
    nx, ny = nx / n, ny / n
    turn = math.degrees(math.atan2(-nx, ny))   # takes a part's local +Y to the normal
    off, r = 0.020 * s, 0.014 * s
    tx, ty = px + nx * off, py + ny * off                             # tube axis
    parts = [c.cylinder(r, z1 - z0, (tx, ty, (z0 + z1) / 2), "Z", "DIA_Glass", segments=6,
                        cap_bottom=False, cap_top=False)]
    for i in range(3):                                                # scale rings
        z = z0 + (z1 - z0) * (i + 1) / 4
        parts.append(c.cylinder(r * 1.25, 0.008 * s, (tx, ty, z), "Z", "DIA_Rubber", segments=6,
                                cap_bottom=False))
    for z in (z0, z1):                                                # fittings into the tank
        block = c.box((1.6 * r, off + 0.012 * s, 0.024 * s), (0, (off - 0.012 * s) / 2, z),
                      "DIA_Steel")
        c.rotate(block, "Z", turn)
        parts.append(c.translate(block, (px, py, 0)))
    return parts


def straps(width, height, z_top, s):
    """Two shoulder straps over the top of the plate and a waist belt, on the X = 0 face."""
    z_low = -0.32 * height
    e = 0.008 * s
    inner = PLATE_FRONT - 0.002
    outline_xz = [(0, z_low), (0, z_top + e), (PLATE_BACK - 0.012, z_top + e),
                  (PLATE_BACK - 0.012, z_top), (inner, z_top), (inner, z_low)]
    outline_zx = [(z, x) for x, z in outline_xz]
    y, w = 0.5 * width - 0.055 * s, 0.05 * s
    one = c.extrude(outline_zx, y - w / 2, y + w / 2, "Y", "DIA_Rubber")
    belt = c.box((-0.002 - inner, width, 0.04 * s), ((inner - 0.002) / 2, 0, -0.36 * height),
                 "DIA_Rubber")          # 2 mm behind the straps' front: no coplanar faces
    return [one, c.mirror(one, "Y"), belt]


def build(size):
    depth, width, height = SIZES[size]
    name = "SM_Backpack_" + size
    s = height / 0.42                     # detail scale, 1.0 for the M size
    top, bottom = height / 2, -height / 2
    c.reset_scene()
    parts = []

    plate_top = top - 0.05 * s
    # Light plate, so the black straps read; narrower than the tank on the one-tank sizes.
    plate_w = width * PLATE_WIDTH[size]
    parts.append(c.box((PLATE_FRONT - PLATE_BACK, plate_w, plate_top - bottom),
                       ((PLATE_FRONT + PLATE_BACK) / 2, 0, (plate_top + bottom) / 2),
                       "DIA_Steel"))
    parts += straps(plate_w, height, plate_top, s)

    zs = top - 0.085 * s                  # tank shoulder; neck and lid above it
    zb = bottom + (0.05 if size == "XL" else 0.04) * s
    body = zs - zb
    front = PLATE_BACK + 0.004            # tanks sink 4 mm into the plate

    if size == "XL":      # two tanks inside the frame (tube 2 cm), its rear rail behind them
        rx, ry, xc, yc = fit_tank(front, -depth + 0.03, 0.003, width / 2 - 0.023, s)
    elif size == "L":     # small canister on the left, the main tank fills the rest
        rs = 0.145 * width
        ys = -(width / 2 - rs * BAND_OUT)
        rx, ry, xc, yc = fit_tank(front, -depth, ys + rs * BAND_OUT + 0.006, width / 2, s)
    else:
        rx, ry, xc, yc = fit_tank(front, -depth, -width / 2, width / 2, s)

    centres = (yc, -yc) if size == "XL" else (yc,)
    for y in centres:
        parts.append(tank(xc, y, rx, ry, zb, zs, s))
        parts.append(lid(xc, y, 0.40 * min(rx, ry), zs + 0.05 * s, top, s))
        parts.append(band(xc, y, rx, ry, zb + 0.12 * body, zb + 0.20 * body))
        if size != "XL":                  # on the XL the frame's upper ring takes this place
            parts.append(band(xc, y, rx, ry, zb + 0.84 * body, zb + 0.92 * body))
    parts += gauge(xc, yc, rx, ry, zb + 0.30 * body, zb + 0.72 * body, s)

    z_out = bottom + 0.016 * s            # hose axis; the hose's underside is the lowest point
    if size == "XL":
        # Manifold under both tanks, hose stub leaving to the rear between them.
        z_out = zb - 0.018 * s
        for y in centres:
            parts.append(c.cylinder(0.024 * s, zb - z_out, (xc, y, (zb + z_out) / 2), "Z",
                                    "DIA_Steel", segments=6, cap_bottom=False, cap_top=False))
        parts.append(c.cylinder(0.016 * s, 2 * yc + 0.04 * s, (xc, 0, z_out), "Y", "DIA_Steel",
                                segments=6, cap_bottom=False, cap_top=False))
        parts.append(c.cylinder(0.018 * s, xc - (-depth + 0.004), ((xc - depth + 0.004) / 2, 0, z_out),
                                "-X", "DIA_Rubber", segments=8, cap_bottom=False))
        # Tube frame: bottom ring, upper ring, rear corner posts.
        t = 0.02
        z_ring = zb + 0.80 * body
        for z in (bottom + t / 2, z_ring):
            for side in (1, -1):
                parts.append(c.box((depth + PLATE_BACK, t, t),
                                   ((PLATE_BACK - depth) / 2, side * (width - t) / 2, z),
                                   "DIA_DarkSteel"))
            parts.append(c.box((t, width - 2 * t, t), (-depth + t / 2, 0, z), "DIA_DarkSteel"))
        for side in (1, -1):
            parts.append(c.box((t * 0.9, t * 0.9, z_ring - bottom - t),
                               (-depth + t / 2, side * (width - t) / 2, (z_ring + bottom) / 2),
                               "DIA_DarkSteel"))
    else:
        parts.append(c.cylinder(0.022 * s, zb - z_out, (xc, yc, (zb + z_out) / 2), "Z", "DIA_Steel",
                                segments=8, cap_top=False))
        y_end = min(yc + 1.05 * ry, width / 2)
        parts.append(c.cylinder(0.016 * s, y_end - yc, (xc, (yc + y_end) / 2, z_out), "Y",
                                "DIA_Rubber", segments=8, cap_bottom=False))

    if size == "L":
        xs = front - rs
        zs_small = zb + 0.58 * body
        parts.append(tank(xs, ys, rs, rs, zb, zs_small, s, mat="DIA_Steel", segments=8))
        parts.append(lid(xs, ys, 0.45 * rs, zs_small + 0.05 * s, zs_small + 0.085 * s, s,
                         mat="DIA_Yellow"))
        small_body = zs_small - zb
        parts.append(band(xs, ys, rs, rs, zb + 0.40 * small_body, zb + 0.56 * small_body,
                          segments=8))
        z_pipe = zb + 0.06 * body          # feed pipe from the canister into the main tank
        parts.append(c.beam((xs, ys, z_pipe), (xc, yc - 0.6 * ry, z_pipe), 0.014 * s, "DIA_Steel",
                            segments=6, cap_start=False, cap_end=False))

    c.assert_outward(parts)
    obj = c.join(parts, name)
    c.export(obj, name)
    c.render_previews(obj, name)
    return obj


if __name__ == "__main__":
    for size in SIZES:
        build(size)
