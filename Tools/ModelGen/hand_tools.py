"""SM_Spoon, SM_Bowl, SM_Trowel, SM_Scoop, SM_Bucket - what the player holds in the hand.

Seen from the first person at arm's length, so they carry more shape than their size
suggests; round sections stay visibly angular (8 - 12 sides). Tools held by a grip
have their origin in the middle of that grip and work towards +X; vessels that can
also stand on the ground have it in the centre of their base.

SM_Spoon (0.20 x 0.045 x 0.018 m; origin = middle of the handle):
  soup spoon. Flat handle, wider at the end, a neck that drops a little and an
  oval bowl whose rim is the lowest working edge. All DIA_Steel.
  Envelope: X [-0.055, 0.144], Y +-0.0225, Z [-0.016, 0.002].

SM_Bowl (0.25 x 0.25 x 0.09 m; origin = centre of the base, on the ground):
  metal mixing bowl, 2.5 l. Flat base 12 cm across, flared wall, a thick rolled
  rim. Inside: floor at Z = 0.012 (radius 0.060), rim at Z = 0.090 (radius 0.117).
  Envelope: X +-0.125, Y +-0.125, Z [0, 0.09].

SM_Trowel (0.28 x 0.12 x 0.092 m; origin = middle of the grip):
  plasterer's finishing trowel. Rectangular steel blade whose underside (the
  working face) lies at Z = -0.075, a rib along it, a shank at the front that
  bends up into an orange grip parallel to the blade.
  Envelope: X +-0.14, Y +-0.06, Z [-0.075, 0.017].

SM_Scoop (0.34 x 0.12 x 0.07 m; origin = middle of the grip):
  metal hand scoop, 1 l. A U-shaped trough with a back wall, its mouth cut on a
  slant so the bottom lip (X = 0.27, Z = -0.035) reaches the asphalt first; a
  ferrule and an orange grip behind it.
  Envelope: X [-0.07, 0.27], Y +-0.06, Z [-0.035, 0.035].

SM_Bucket (0.29 x 0.31 x 0.39 m; origin = centre of the base, on the ground):
  metal bucket, 10 l. Tapered body with two rolled hoops and a rim, two ears at
  Y = +-0.147 (Z = 0.215) and a wire bail standing upright over the bucket with
  an orange grip at its top (0, 0, 0.37). Tilting about the line through the ears
  pours towards +X. Inside: floor at Z = 0.012 (radius 0.096), rim radius 0.132.
  Envelope: X +-0.144, Y +-0.155, Z [0, 0.385].

Run:  blender -b -P Tools/ModelGen/hand_tools.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

METAL, DARK, GRIP = "DIA_Steel", "DIA_DarkSteel", "DIA_Orange"


def build_spoon():
    c.reset_scene()
    parts = []
    # Handle: flat wide end, then a narrow neck dropping to the bowl.
    parts.append(c.box((0.050, 0.016, 0.003), (-0.030, 0, 0.0005), METAL))
    parts.append(c.bar((-0.008, 0, 0.0005), (0.082, 0, -0.0035), 0.009, 0.003, METAL))
    # Bowl: a shallow dish with wall thickness, stretched into an oval.
    centre = (0.110, 0, -0.003)
    bowl = c.lathe([(0.0, -0.013), (0.013, -0.011), (0.0225, -0.001), (0.0225, 0.001),
                    (0.019, 0.001), (0.011, -0.008), (0.0, -0.009)], centre, "Z", METAL, segments=8)
    c.scale(bowl, (1.5, 1.0, 1.0), pivot=centre)
    parts.append(bowl)
    c.finish(parts, "SM_Spoon", silhouette=False)


def build_bowl():
    c.reset_scene()
    parts = [c.lathe([(0.0, 0.0), (0.060, 0.0), (0.066, 0.006), (0.121, 0.080), (0.125, 0.080),
                      (0.125, 0.090), (0.117, 0.090), (0.060, 0.012), (0.0, 0.012)],
                     (0, 0, 0), "Z", METAL, segments=12)]
    # A dark band under the rim: the rolled edge reads as a separate ring.
    parts.append(c.ring(0.116, 0.1235, 0.073, 0.079, (0, 0, 0), "Z", DARK, 12))
    c.finish(parts, "SM_Bowl", silhouette=False)


def build_trowel():
    c.reset_scene()
    parts = []
    blade_z = -0.073
    parts.append(c.box((0.28, 0.12, 0.004), (0, 0, blade_z), METAL))
    parts.append(c.box((0.20, 0.014, 0.007), (0.0, 0, blade_z + 0.0055), DARK))          # rib
    # Shank: up from the front of the rib, back into the grip.
    parts.append(c.beam((0.078, 0, blade_z + 0.006), (0.090, 0, 0.0), 0.006, DARK, segments=6))
    parts.append(c.beam((0.094, 0, 0.0), (0.055, 0, 0.0), 0.006, DARK, segments=6))
    parts.append(c.cylinder(0.010, 0.012, (0.066, 0, 0), "X", METAL, segments=8))        # ferrule
    # Grip: slightly barrel-shaped, eight-sided.
    parts.append(c.lathe([(0.0, -0.062), (0.012, -0.060), (0.017, -0.030), (0.017, 0.030),
                          (0.013, 0.060), (0.0, 0.060)], (0, 0, 0), "X", GRIP, segments=8))
    c.finish(parts, "SM_Trowel", silhouette=False)


def build_scoop():
    c.reset_scene()
    parts = []
    half, low, top, wall = 0.060, -0.035, 0.035, 0.004
    chamfer = 0.016
    outer = [(-half, top), (-half, low + chamfer), (-half + chamfer, low),
             (half - chamfer, low), (half, low + chamfer), (half, top)]
    inner = [(half - wall, top), (half - wall, low + chamfer + 0.002), (half - chamfer - 0.002, low + wall),
             (-half + chamfer + 0.002, low + wall), (-half + wall, low + chamfer + 0.002), (-half + wall, top)]
    back_x, front_x = 0.070, 0.270
    trough = c.extrude(outer + inner, back_x, front_x, "X", METAL)
    # Slanted mouth: the top edge stops short, the bottom lip leads.
    for v in trough.data.vertices:
        if v.co.x > (back_x + front_x) / 2:
            v.co.x -= 0.8 * (v.co.z - low)
    parts.append(trough)
    parts.append(c.extrude(outer, back_x - 0.004, back_x + 0.001, "X", METAL))           # back wall
    # Ferrule and grip along X.
    parts.append(c.cone(0.012, 0.022, 0.030, (0.052, 0, 0), "X", DARK, segments=8, cap_top=False))
    parts.append(c.lathe([(0.0, -0.070), (0.013, -0.067), (0.017, -0.030), (0.017, 0.020),
                          (0.013, 0.038), (0.0, 0.038)], (0, 0, 0), "X", GRIP, segments=8))
    c.finish(parts, "SM_Scoop", silhouette=False)


def build_bucket():
    c.reset_scene()
    parts = []
    r0, r1, h = 0.100, 0.138, 0.236

    def radius(z):
        return r0 + (r1 - r0) * z / h

    parts.append(c.lathe([(0.0, 0.0), (r0, 0.0), (r1, h), (0.144, h), (0.144, 0.244), (0.132, 0.244),
                          (0.096, 0.012), (0.0, 0.012)], (0, 0, 0), "Z", METAL, segments=12))
    for z in (0.075, 0.155):                                                             # rolled hoops
        parts.append(c.ring(radius(z) - 0.003, radius(z) + 0.004, z - 0.006, z + 0.006, (0, 0, 0), "Z", DARK, 12))
    # Ears and the bail: a half ellipse of wire in the Y-Z plane, upright.
    ear_z, ear_y, rise = 0.215, 0.147, 0.155
    for side in (1, -1):
        parts.append(c.box((0.034, 0.014, 0.040), (0, side * ear_y, ear_z), DARK))
    steps = 8
    pts = [(0.0, ear_y * math.cos(math.pi * i / steps), ear_z + rise * math.sin(math.pi * i / steps))
           for i in range(steps + 1)]
    for a, b in zip(pts, pts[1:]):
        parts.append(c.beam(a, b, 0.0045, DARK, segments=6))
    parts.append(c.cylinder(0.014, 0.11, (0, 0, ear_z + rise), "Y", GRIP, segments=8))
    c.finish(parts, "SM_Bucket", silhouette=False)


build_spoon()
build_bowl()
build_trowel()
build_scoop()
build_bucket()
