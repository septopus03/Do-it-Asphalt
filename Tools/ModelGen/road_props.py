"""SM_RoadStub and SM_Barrier - the old road behind each road end and its closure.

SM_RoadStub (12.0 x 3.4 x 0.05 m; origin = centre of the near edge, on the ground;
the road runs towards +X, X [0, 12.0]):
  a low strip of DIA_Asphalt with a dashed white centre line (six 1 m dashes) and a
  DIA_Concrete kerb along each side; the kerbs are the 5 cm high points and lie
  inside the 3.4 m width. Envelope: X [0, 12], Y +-1.7, Z [0, 0.05]. 3 materials.

SM_Barrier (0.3 x 3.4 x 1.0 m; origin = centre of the footprint, on the ground):
  fixed road closure across the full road width: two concrete feet, two dark
  posts and two boards with slanted red / white stripes (the same on both faces).
  Envelope: X +-0.15, Y +-1.7, Z [0, 1.0]. 4 materials.

Run:  blender -b -P Tools/ModelGen/road_props.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

ROAD_HALF_WIDTH = 1.7                # both models span the 3.4 m road

# --- road stub --------------------------------------------------------------
ROAD_LENGTH = 12.0
KERB_WIDTH = 0.20
KERB_HEIGHT = 0.05                   # the envelope height
KERB_CHAMFER = 0.02                  # sloped inner edge of the kerb
ASPHALT_TOP = 0.03
DASH = (1.0, 0.12, 0.01)             # length, width, height above the asphalt
DASH_PITCH = 2.0                     # 1 m dash + 1 m gap -> 6 dashes on 12 m

# --- barrier ----------------------------------------------------------------
FOOT = (0.30, 0.40, 0.12)            # X x Y x Z; its X size is the envelope depth
POST_Y = 1.25
POST = (0.09, 0.07)                  # X x Y, thicker than the boards so it shows on both faces
POST_BELOW_TOP = 0.03                # the posts stop short of the upper board's top edge
BOARD_THICK = 0.05
BOARDS_Z = ((0.70, 1.00), (0.30, 0.48))   # upper and lower board, bottom - top
STRIPE = 0.34                        # width of one colour along the board (10 per board)


def clip_u(poly, lo, hi):
    """Clip a convex polygon [(u, v), ...] to the band lo <= u <= hi."""
    for sign, limit in ((1.0, hi), (-1.0, lo)):
        out = []
        for i, a in enumerate(poly):
            b = poly[(i + 1) % len(poly)]
            a_in, b_in = sign * a[0] <= sign * limit, sign * b[0] <= sign * limit
            if a_in:
                out.append(a)
            if a_in != b_in:
                t = (limit - a[0]) / (b[0] - a[0])
                out.append((limit, a[1] + t * (b[1] - a[1])))
        poly = out
    return poly


def striped_board(z0, z1, colours):
    """Board across the road between heights z0 and z1, made of 45 degree stripes
    that alternate between the two `colours`. Each stripe is its own prism."""
    slant = z1 - z0
    parts = []
    k = 0
    start = -ROAD_HALF_WIDTH - slant          # the first stripe only shows its top corner
    while start + k * STRIPE < ROAD_HALF_WIDTH:
        y = start + k * STRIPE
        stripe = clip_u([(y, z0), (y + STRIPE, z0), (y + STRIPE + slant, z1), (y + slant, z1)],
                        -ROAD_HALF_WIDTH, ROAD_HALF_WIDTH)
        if len(stripe) >= 3:
            parts.append(c.extrude(stripe, -BOARD_THICK / 2, BOARD_THICK / 2, "X", colours[k % 2]))
        k += 1
    return parts


def build_road_stub():
    name = "SM_RoadStub"
    c.reset_scene()
    parts = []

    inner = ROAD_HALF_WIDTH - KERB_WIDTH
    parts.append(c.box((ROAD_LENGTH, 2 * inner, ASPHALT_TOP),
                       (ROAD_LENGTH / 2, 0, ASPHALT_TOP / 2), "DIA_Asphalt"))

    length, width, height = DASH
    x = (DASH_PITCH - length) / 2             # half a gap before the first dash
    while x + length <= ROAD_LENGTH:
        parts.append(c.box((length, width, height),
                           (x + length / 2, 0, ASPHALT_TOP + height / 2), "DIA_White"))
        x += DASH_PITCH

    kerb = c.extrude([(inner, 0.0), (ROAD_HALF_WIDTH, 0.0), (ROAD_HALF_WIDTH, KERB_HEIGHT),
                      (inner + KERB_CHAMFER, KERB_HEIGHT), (inner, KERB_HEIGHT - KERB_CHAMFER)],
                     0.0, ROAD_LENGTH, "X", "DIA_Concrete")
    parts += [kerb, c.mirror(kerb, "Y")]

    return c.finish(parts, name)


def build_barrier():
    name = "SM_Barrier"
    c.reset_scene()
    parts = []

    top = BOARDS_Z[0][1] - POST_BELOW_TOP
    for side in (1, -1):
        y = side * POST_Y
        parts.append(c.box(FOOT, (0, y, FOOT[2] / 2), "DIA_Concrete"))
        parts.append(c.box((POST[0], POST[1], top - FOOT[2]), (0, y, (top + FOOT[2]) / 2),
                           "DIA_DarkSteel"))
    for z0, z1 in BOARDS_Z:
        parts += striped_board(z0, z1, ("DIA_Red", "DIA_White"))

    return c.finish(parts, name)


if __name__ == "__main__":
    build_road_stub()
    build_barrier()
