"""SM_Helmet - yellow construction hard hat, 0.28 x 0.24 x 0.16 m.

Origin: centre of the bottom rim of the shell (where the head goes), Z = 0 at the
rim. A round shell 0.24 m across with a narrow brim all round, a peak (visor)
reaching forward to X = +0.16 and a raised crest running front to back over the
top. Envelope: X [-0.12, 0.16], Y +-0.12, Z [0, 0.16]. One material, DIA_Yellow.

Run:  blender -b -P Tools/ModelGen/helmet.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_Helmet"
MAT = "DIA_Yellow"

R_BRIM = 0.12        # outer radius of the brim = half of the 0.24 m width
R_SHELL = 0.106      # shell radius where it meets the brim
Z_SHELL = 0.020      # the dome starts here
DOME_H = 0.130       # dome top at 0.150; the crest adds 1 cm -> 0.16
PEAK_X = 0.16


def dome_point(angle_deg, offset=0.0):
    """(r, z) on the dome outline, `offset` metres outside it; 0 deg = rim, 90 = top."""
    a = math.radians(angle_deg)
    return ((R_SHELL + offset) * math.cos(a), Z_SHELL + (DOME_H + offset) * math.sin(a))


c.reset_scene()
parts = []

# Shell with brim: one closed solid of revolution (flat underside).
profile = [(R_BRIM, 0.0), (R_BRIM, 0.010), (R_SHELL, Z_SHELL)]
profile += [dome_point(a) for a in (30, 55, 75)] + [(0.0, Z_SHELL + DOME_H)]
parts.append(c.lathe(profile, (0, 0, 0), "Z", MAT, segments=16))

# Peak: flat plate growing out of the front of the brim towards +X.
peak_xy = [(0.05, -0.105), (0.112, -0.085), (0.146, -0.048), (PEAK_X, 0.0),
           (0.146, 0.048), (0.112, 0.085), (0.05, 0.105)]
parts.append(c.extrude(peak_xy, 0.0, 0.014, "Z", MAT))

# Crest: a raised rib over the top from front to back (an arc in the X-Z plane).
angles = (12, 38, 64, 90, 116, 142, 168)
outer = [dome_point(a, 0.010) for a in angles]            # (x, z): front -> top -> back
inner = [dome_point(a, -0.014) for a in reversed(angles)]
parts.append(c.extrude([(z, x) for x, z in outer + inner], -0.017, 0.017, "Y", MAT))

c.assert_outward(parts)
obj = c.join(parts, NAME)
c.export(obj, NAME)
c.render_previews(obj, NAME)
