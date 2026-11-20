"""SM_Mall - the shopping centre in the corner cut-out of the car park, 100 x 100 x 14.3 m.

Origin: centre of the footprint, on the ground. The footprint is exactly the
100 x 100 m cut-out (X +-50, Y +-50) and nothing sticks out of it, because the
asphalt cells start right at the wall. The two facades that face the car park
are the -X side and the -Y side; the +X and +Y sides lie on the edge of the map
and are plain.

Each car-park facade (described along its length s, from the corner tower at
s = -38 to the far end at s = +50; the wall plane is 0.3 m inside the footprint
and the facade elements fill that 0.3 m):
  dark plinth, concrete pilasters every 10 m, a white fascia band under the
  roof line; shop windows in the bays from s = -20 to 30; the entrance in the
  bay s = 0..10 - glazing down to the ground with door frames and push bars at
  hand height, an orange lintel - and above it a raised white sign board 16 x 4 m
  whose centre is at s = 5, Z = 10.6 (the game writes the name on it).
The corner at (-50, -50), which looks at the inner corner of the car park, is a
12 x 12 m tower, 14.3 m high: a strip of glazing on both faces, a white band and
an orange cap. On the roof a lift head, air handling units and two skylights.
Envelope: X +-50, Y +-50, Z [0, 14.3]. 5 materials.

Run:  blender -b -P Tools/ModelGen/mall.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_Mall"
WALL, WHITE, GLASS, DARK, ACCENT = "DIA_Concrete", "DIA_White", "DIA_Glass", "DIA_DarkSteel", "DIA_Orange"

HALF = 50.0
DEPTH = 0.30                         # facade relief in front of the wall plane
WALL_AT = -(HALF - DEPTH)            # the wall plane of a car-park facade
ROOF_Z = 10.6
TOWER = 12.0                         # tower side
TOWER_Z = 14.0
S0, S1 = -HALF + TOWER, HALF         # a facade runs from the tower to the far end
PLINTH_Z = 0.6
FASCIA_Z = 8.6
ENTRY = (0.0, 10.0)                  # entrance bay along the facade

c.reset_scene()
parts = []


def fbox(face, s0, s1, d0, d1, z0, z1, mat):
    """Box on a car-park facade: s along it, d out of the wall plane (0..DEPTH), z up.
    face 'X' is the -X side (s = Y), face 'Y' the -Y side (s = X)."""
    out0, out1 = WALL_AT - d1, WALL_AT - d0
    size_s, size_d, size_z = s1 - s0, out1 - out0, z1 - z0
    sm, dm, zm = (s0 + s1) / 2, (out0 + out1) / 2, (z0 + z1) / 2
    if face == "X":
        parts.append(c.box((size_d, size_s, size_z), (dm, sm, zm), mat))
    else:
        parts.append(c.box((size_s, size_d, size_z), (sm, dm, zm), mat))


# Main block: its +X / +Y faces are the footprint, the car-park faces are the wall plane.
lo, hi = WALL_AT, HALF
parts.append(c.box((hi - lo, hi - lo, ROOF_Z), ((lo + hi) / 2, (lo + hi) / 2, ROOF_Z / 2), WALL))

for face in ("X", "Y"):
    # Plinth, interrupted by the entrance bay (its pilasters run to the ground).
    fbox(face, S0, ENTRY[0] - 0.4, 0, DEPTH, 0, PLINTH_Z, DARK)
    fbox(face, ENTRY[1] + 0.4, S1, 0, DEPTH, 0, PLINTH_Z, DARK)
    # Pilasters every 10 m.
    for s in range(-30, 50, 10):
        ground = 0.0 if s in ENTRY else PLINTH_Z
        fbox(face, s - 0.4, s + 0.4, 0, DEPTH, ground, FASCIA_Z, WALL)
    fbox(face, S1 - 0.5, S1, 0, DEPTH, PLINTH_Z, FASCIA_Z, WALL)                    # end pier
    # Fascia under the roof line.
    fbox(face, S0, S1, 0, DEPTH, FASCIA_Z, ROOF_Z, WHITE)
    # Shop windows: glass, a dark transom above, a mullion in the middle.
    for s in (-20, -10, 10, 20):
        a, b = s + 0.4, s + 9.6
        fbox(face, a, b, 0, 0.08, PLINTH_Z, 4.0, GLASS)
        fbox(face, a, b, 0, 0.14, 4.0, 4.25, DARK)
        fbox(face, s + 4.93, s + 5.07, 0.08, 0.14, PLINTH_Z, 4.0, DARK)
    # Entrance: glazing to the ground, door frames, push bars, orange lintel.
    a, b = ENTRY[0] + 0.4, ENTRY[1] - 0.4
    fbox(face, a, b, 0, 0.08, 0, 4.6, GLASS)
    fbox(face, a, b, 0, DEPTH, 4.6, 5.1, ACCENT)
    fbox(face, a, b, 0.08, 0.14, 2.30, 2.42, DARK)                                 # door head
    for i in range(1, 9):
        s = a + (b - a) * i / 9.0
        fbox(face, s - 0.05, s + 0.05, 0.08, 0.14, 0, 4.6 if i % 3 == 0 else 2.30, DARK)
    leaf = (b - a) / 9.0
    for i in (3, 4, 5):                                                            # push bars on the middle doors
        s = a + leaf * i
        fbox(face, s + 0.18, s + leaf - 0.18, 0.14, 0.19, 1.00, 1.07, DARK)
    # Raised sign board above the entrance, orange coping on it.
    fbox(face, ENTRY[0] - 3.0, ENTRY[1] + 3.0, 0, DEPTH, ROOF_Z, ROOF_Z + 2.0, WHITE)
    fbox(face, ENTRY[0] - 3.0, ENTRY[1] + 3.0, 0, DEPTH, ROOF_Z + 2.0, ROOF_Z + 2.3, ACCENT)

# Corner tower: body just inside the footprint, glazing strips, white band, orange cap.
t_lo, t_hi = -HALF + 0.15, -HALF + TOWER
t_mid = (t_lo + t_hi) / 2
parts.append(c.box((t_hi - t_lo, t_hi - t_lo, 11.0), (t_mid, t_mid, 5.5), WALL))
strip = (-HALF + 0.10, -HALF + TOWER / 2)                                          # out, along
parts.append(c.box((0.10, 5.0, 9.0), (strip[0], strip[1], 5.6), GLASS))            # 5 cm behind the floor lines
parts.append(c.box((5.0, 0.10, 9.0), (strip[1], strip[0], 5.6), GLASS))
for z in (3.35, 5.6, 7.85):                                                        # floor lines across the glazing
    parts.append(c.box((0.20, 5.0, 0.16), (-HALF + 0.10, strip[1], z), DARK))
    parts.append(c.box((5.0, 0.20, 0.16), (strip[1], -HALF + 0.10, z), DARK))
b_lo, b_hi = -HALF, -HALF + TOWER + 0.15
b_mid = (b_lo + b_hi) / 2
parts.append(c.box((b_hi - b_lo, b_hi - b_lo, TOWER_Z - 11.0), (b_mid, b_mid, (TOWER_Z + 11.0) / 2), WHITE))
parts.append(c.box((b_hi - b_lo, b_hi - b_lo, 0.3), (b_mid, b_mid, TOWER_Z + 0.15), ACCENT))

# Roof: lift head, air handling units, two skylights.
parts.append(c.box((6.0, 6.0, 3.0), (10.0, 10.0, ROOF_Z + 1.5), WHITE))
for x, y in ((-20, 20), (-12, 20), (22, -18), (30, -18), (32, 30), (-25, -20)):
    parts.append(c.box((5.0, 3.0, 1.8), (x, y, ROOF_Z + 0.9), DARK))
    parts.append(c.box((1.6, 1.6, 0.5), (x, y, ROOF_Z + 2.05), WALL))
for y in (-30.0, 35.0):
    parts.append(c.extrude([(y - 2.5, ROOF_Z), (y + 2.5, ROOF_Z), (y, ROOF_Z + 1.6)], -5.0, 30.0, "X", GLASS,
                           ))

c.finish(parts, NAME, front=(-1.0, -0.6, 0.30))   # look at the car-park facades and the tower
