"""SM_Trough, SM_Curb, SM_PaverCar_WideBar - things that stand on or around the car park.

SM_Trough (1.28 x 0.60 x 0.50 m; origin = centre of the footprint, on the ground):
  the starting dispensing trough the player scoops from. A steel tub with sloping
  walls on four dark legs, filled with asphalt: a rough dark surface at Z = 0.44
  with a few low lumps. An orange carrying handle on each short end.
  Envelope: X +-0.64, Y +-0.30, Z [0, 0.50]. 4 materials.

SM_Curb (0.15 x 2.0 x 0.12 m; origin = centre of the footprint, on the ground):
  one concrete kerbstone. Pieces tile along Y at a 2.0 m pitch; the chamfered top
  edge is on the +X side, which faces the car park.
  Envelope: X +-0.075, Y +-1.0, Z [0, 0.12]. 1 material.

SM_PaverCar_WideBar (0.14 x 3.5 x 0.17 m; origin = the ORIGIN OF SM_PaverCar):
  the wider spreader bar bought as the truck's upgrade. It is attached to the
  truck without any offset and swallows the truck's own 3.0 m bar (X -2.30..-2.18,
  Z 0.06..0.18): a steel bar 3.5 m wide with thirteen nozzles and an orange plate
  on each end.
  Envelope: X [-2.31, -2.17], Y +-1.75, Z [0.02, 0.19]. 3 materials.

Run:  blender -b -P Tools/ModelGen/lot_props.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

METAL, FRAME, TYRE, GRIP = "DIA_Steel", "DIA_DarkSteel", "DIA_Rubber", "DIA_Orange"


def build_trough():
    c.reset_scene()
    parts = []
    z0, z1 = 0.12, 0.50
    parts.append(c.tub((1.20, 0.60), (1.08, 0.50), z0, z1, 0.012, 0.012, (0, 0), METAL))
    # Asphalt: a slab that reaches into the walls, with low lumps on it.
    parts.append(c.box((1.165, 0.568, 0.020), (0, 0, 0.430), "DIA_Asphalt"))
    for x, y, r, h in ((-0.30, 0.06, 0.17, 0.045), (0.12, -0.08, 0.14, 0.035), (0.36, 0.09, 0.11, 0.030)):
        parts.append(c.dome(r, h, (x, y, 0.439), "Z", "DIA_Asphalt", segments=6, rings=2, cap=False))
    # Legs with a brace across each pair.
    for sx in (1, -1):
        for sy in (1, -1):
            parts.append(c.box((0.05, 0.05, 0.13), (sx * 0.48, sy * 0.20, 0.065), FRAME))
        parts.append(c.box((0.03, 0.35, 0.03), (sx * 0.48, 0, 0.045), FRAME))
        # carrying handle on the short end
        for sy in (1, -1):
            parts.append(c.box((0.050, 0.028, 0.028), (sx * 0.605, sy * 0.095, 0.420), FRAME))
        parts.append(c.box((0.028, 0.230, 0.030), (sx * 0.626, 0, 0.420), GRIP))
    c.finish(parts, "SM_Trough")


def build_curb():
    c.reset_scene()
    # Outline in the Z-X plane (extrude along Y takes (u, v) = (z, x)).
    parts = [c.extrude([(0.0, -0.075), (0.0, 0.075), (0.095, 0.075), (0.12, 0.050), (0.12, -0.075)],
                       -1.0, 1.0, "Y", "DIA_Concrete")]
    c.finish(parts, "SM_Curb")


def build_wide_bar():
    c.reset_scene()
    parts = []
    bar_x, bar_z, half_y = (-2.31, -2.17), (0.05, 0.19), 1.74
    xm, zm = (bar_x[0] + bar_x[1]) / 2, (bar_z[0] + bar_z[1]) / 2
    parts.append(c.box((bar_x[1] - bar_x[0], 2 * half_y, bar_z[1] - bar_z[0]), (xm, 0, zm), METAL))
    count, tip = 13, 0.02
    for i in range(count):
        y = (half_y - 0.10) * (2.0 * i / (count - 1) - 1.0)
        parts.append(c.cone(0.025, 0.045, bar_z[0] - tip, (xm, y, (bar_z[0] + tip) / 2), "-Z", TYRE,
                            segments=6, cap_bottom=False))
    for sy in (1, -1):
        parts.append(c.box((0.14, 0.02, 0.14), (xm, sy * (half_y + 0.0), zm), GRIP))
    c.finish(parts, "SM_PaverCar_WideBar")


build_trough()
build_curb()
build_wide_bar()
