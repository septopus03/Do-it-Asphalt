"""SM_AutoPaver - the small autonomous paving robot, 1.0 x 0.8 x 0.6 m.

Origin: centre of the footprint, on the ground; it drives towards +X.
A low yellow body with a chamfered nose sits on two rubber tracks (three steel
wheels showing in each). Two big round glass "eyes" on the nose make it friendly.
In front of the tracks, just above the ground, the steel spreader bar: 0.8 m
wide, at X = BAR_X, carried by two arms and fed by a hose from the hull. On the
roof an orange beacon and, at the rear right corner, a whip antenna with an
orange tip whose top is the highest point (Z = 0.6).
Envelope: X +-0.5, Y +-0.4, Z [0, 0.6]. 5 materials.

Run:  blender -b -P Tools/ModelGen/auto_paver.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_AutoPaver"
BODY, TRACK, METAL, GLASS, ACCENT = "DIA_Yellow", "DIA_Rubber", "DIA_Steel", "DIA_Glass", "DIA_Orange"

# --- tracks -----------------------------------------------------------------
TRACK_R = 0.11                       # end radius; the track is 2 * TRACK_R high
TRACK_X = (-0.39, 0.25)              # centres of the rear and front end arcs
TRACK_Y = (0.24, 0.38)               # inner and outer face of the right-hand track
TRACK_ARC_STEPS = 6                  # 30 deg steps: a vertex lands exactly on X = -0.5 and Z = 0
WHEEL_R = 0.075
WHEEL_STANDOUT = 0.008               # how far the wheel discs stand off the track's outer face
WHEEL_XS = (TRACK_X[0], (TRACK_X[0] + TRACK_X[1]) / 2, TRACK_X[1])
CLEAT_XS = (-0.36, -0.22, -0.07, 0.08, 0.22)
CLEAT_H = 0.012                      # stays under the mudguard (BODY_Z0)

# --- body -------------------------------------------------------------------
HULL_X = (-0.40, 0.30)               # lower hull between the tracks
HULL_Z = (0.06, 0.25)                # its top ends just inside the upper body
BODY_Z0, BODY_Z1 = 0.24, 0.44        # upper body: also the mudguards over the tracks
NOSE_X = 0.34                        # vertical front face carrying the eyes
# Upper body outline in the X-Z plane (chamfered nose and tail).
BODY_XZ = [(-0.46, BODY_Z0), (NOSE_X, BODY_Z0), (NOSE_X, 0.39), (0.29, BODY_Z1),
           (-0.40, BODY_Z1), (-0.46, 0.38)]
BODY_HALF_Y = TRACK_Y[1]

EYE_Y, EYE_Z = 0.13, 0.315
EYE_R, BEZEL_R, PUPIL_R = 0.055, 0.068, 0.024
BEZEL_H, EYE_H, PUPIL_H = 0.008, 0.012, 0.016    # how far each stands off the nose

# --- spreader bar (front) ---------------------------------------------------
BAR_X = 0.47                         # centre line of the bar
BAR_HALF_Y = 0.40                    # the bar is the envelope width
BAR_Z = (0.04, 0.10)
BAR_DEPTH = 0.05
END_PLATE = (0.06, 0.02, 0.08)       # orange plates closing the bar; front face at X = 0.5
ARM_Y = 0.15
NOZZLE_YS = (-0.30, -0.15, 0.0, 0.15, 0.30)
NOZZLE_TIP_Z = 0.015                 # nozzles end just above the ground

# --- roof -------------------------------------------------------------------
BEACON_XY = (-0.06, 0.0)
BEACON_BASE = (0.058, 0.02)          # radius, height
BEACON_R, BEACON_H, BEACON_DOME_H = 0.042, 0.06, 0.03
FILLER_XY = (0.16, 0.0)
ANTENNA_XY = (-0.36, 0.28)
ANTENNA_TIP = 0.03                   # orange cube on top
TOP_Z = 0.60                         # envelope height = top of the antenna tip


def track_outline():
    """Stadium outline of a track in the X-Z plane, as extrude()'s (z, x) pairs for
    axis 'Y': rear half circle top -> bottom, then front half circle bottom -> top."""
    pts = []
    for centre_x, start_deg in ((TRACK_X[0], 90.0), (TRACK_X[1], 270.0)):
        for k in range(TRACK_ARC_STEPS + 1):
            a = math.radians(start_deg + 180.0 * k / TRACK_ARC_STEPS)
            pts.append((TRACK_R + TRACK_R * math.sin(a), centre_x + TRACK_R * math.cos(a)))
    return pts


c.reset_scene()
parts = []

# Tracks with cleats on the top run and three wheel discs on the outer face.
for side in (1, -1):
    outward = "Y" if side > 0 else "-Y"
    y_mid = side * (TRACK_Y[0] + TRACK_Y[1]) / 2
    parts.append(c.extrude(track_outline(), side * TRACK_Y[0], side * TRACK_Y[1], "Y", TRACK))
    for x in CLEAT_XS:
        parts.append(c.box((0.03, TRACK_Y[1] - TRACK_Y[0], CLEAT_H),
                           (x, y_mid, 2 * TRACK_R + CLEAT_H / 2 - 0.002), TRACK))
    for x in WHEEL_XS:
        parts.append(c.cylinder(WHEEL_R, WHEEL_STANDOUT,
                                (x, side * (TRACK_Y[1] + WHEEL_STANDOUT / 2), TRACK_R),
                                outward, METAL, segments=12, cap_bottom=False))

# Lower hull between the tracks and the upper body spanning both of them.
parts.append(c.box((HULL_X[1] - HULL_X[0], 2 * TRACK_Y[0], HULL_Z[1] - HULL_Z[0]),
                   ((HULL_X[0] + HULL_X[1]) / 2, 0, (HULL_Z[0] + HULL_Z[1]) / 2), BODY))
parts.append(c.extrude([(z, x) for x, z in BODY_XZ], -BODY_HALF_Y, BODY_HALF_Y, "Y", BODY))

# Eyes on the nose: steel bezel, glass lens, dark pupil.
for side in (1, -1):
    for radius, standout, mat in ((BEZEL_R, BEZEL_H, METAL), (EYE_R, EYE_H, GLASS),
                                  (PUPIL_R, PUPIL_H, TRACK)):
        parts.append(c.cylinder(radius, standout, (NOSE_X + standout / 2, side * EYE_Y, EYE_Z),
                                "X", mat, segments=12, cap_bottom=False))

# Spreader bar in front of the tracks: bar, end plates, two arms, feed hose, nozzles.
bar_zm = (BAR_Z[0] + BAR_Z[1]) / 2
parts.append(c.box((BAR_DEPTH, 2 * (BAR_HALF_Y - END_PLATE[1]), BAR_Z[1] - BAR_Z[0]),
                   (BAR_X, 0, bar_zm), METAL))
for side in (1, -1):
    parts.append(c.box(END_PLATE, (BAR_X, side * (BAR_HALF_Y - END_PLATE[1] / 2), bar_zm), ACCENT))
    parts.append(c.bar((HULL_X[1] - 0.02, side * ARM_Y, 0.13), (BAR_X, side * ARM_Y, bar_zm),
                       0.04, 0.04, METAL))
parts.append(c.beam((HULL_X[1] - 0.02, 0, HULL_Z[1] - 0.05), (BAR_X, 0, BAR_Z[1] - 0.01), 0.018, TRACK,
                    segments=6))
for y in NOZZLE_YS:
    parts.append(c.cone(0.012, 0.022, BAR_Z[0] - NOZZLE_TIP_Z,
                        (BAR_X, y, (BAR_Z[0] + NOZZLE_TIP_Z) / 2), "-Z", TRACK, segments=6,
                        cap_bottom=False))

# Roof: beacon, filler cap, antenna.
bx, by = BEACON_XY
z = BODY_Z1
parts.append(c.cylinder(BEACON_BASE[0], BEACON_BASE[1], (bx, by, z + BEACON_BASE[1] / 2), "Z", METAL,
                        segments=12, cap_bottom=False))
z += BEACON_BASE[1]
parts.append(c.cylinder(BEACON_R, BEACON_H, (bx, by, z + BEACON_H / 2), "Z", ACCENT, segments=12,
                        cap_bottom=False, cap_top=False))
parts.append(c.dome(BEACON_R, BEACON_DOME_H, (bx, by, z + BEACON_H), "Z", ACCENT, segments=12,
                    rings=2, cap=False))
parts.append(c.cylinder(0.05, 0.02, (FILLER_XY[0], FILLER_XY[1], BODY_Z1 + 0.01), "Z", METAL,
                        segments=8, cap_bottom=False))
ax, ay = ANTENNA_XY
parts.append(c.cylinder(0.022, 0.02, (ax, ay, BODY_Z1 + 0.01), "Z", METAL, segments=6,
                        cap_bottom=False))
parts.append(c.beam((ax, ay, BODY_Z1), (ax, ay, TOP_Z - ANTENNA_TIP / 2), 0.007, METAL, segments=5,
                    cap_start=False, cap_end=False))
parts.append(c.box((ANTENNA_TIP,) * 3, (ax, ay, TOP_Z - ANTENNA_TIP / 2), ACCENT))

c.finish(parts, NAME)
