"""SM_Cone - traffic cone with a white stripe, 0.3 x 0.3 x 0.5 m.

Origin: centre of the footprint, on the ground. 118 triangles, 2 materials.

Run:  blender -b -P Tools/ModelGen/cone.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_Cone"

BASE = 0.30          # square base plate side
BASE_H = 0.03
FLANGE_H = 0.02      # short collar where the cone meets the plate
BODY_Z0 = BASE_H + FLANGE_H
TOP_Z = 0.50
R_FLANGE = 0.135
R_BOTTOM = 0.105
R_TOP = 0.028
STRIPE = (0.24, 0.36)  # white band, z from - to


def radius_at(z):
    t = (z - BODY_Z0) / (TOP_Z - BODY_Z0)
    return R_BOTTOM + (R_TOP - R_BOTTOM) * t


def ring(z0, z1, mat, cap_top=False):
    """One slice of the cone body between heights z0 and z1."""
    return c.cone(radius_at(z0), radius_at(z1), z1 - z0, (0, 0, (z0 + z1) / 2), "Z", mat,
                  cap_bottom=False, cap_top=cap_top)


c.reset_scene()
parts = [
    c.box((BASE, BASE, BASE_H), (0, 0, BASE_H / 2), "DIA_Orange"),
    c.cone(R_FLANGE, R_BOTTOM, FLANGE_H, (0, 0, BASE_H + FLANGE_H / 2), "Z", "DIA_Orange",
           cap_bottom=False, cap_top=False),
    ring(BODY_Z0, STRIPE[0], "DIA_Orange"),
    ring(STRIPE[0], STRIPE[1], "DIA_White"),
    ring(STRIPE[1], TOP_Z, "DIA_Orange", cap_top=True),
]
obj = c.join(parts, NAME)
c.export(obj, NAME)
c.render_previews(obj, NAME)
