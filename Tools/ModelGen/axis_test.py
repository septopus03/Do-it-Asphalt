"""SM_AxisTest - proves the axis and scale conversion of the pipeline.

An asymmetric box, 1.0 (X) x 0.5 (Y) x 0.25 (Z) m, origin in its corner, lying
entirely in +X, +Y, +Z. After import into Unreal the centre of its bounds must be
positive on all three axes. Not used in the game.

Run:  blender -b -P Tools/ModelGen/axis_test.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

NAME = "SM_AxisTest"

c.reset_scene()
obj = c.join([c.box((1.0, 0.5, 0.25), (0.5, 0.25, 0.125), "DIA_Yellow")], NAME)
c.export(obj, NAME)
c.render_previews(obj, NAME)
