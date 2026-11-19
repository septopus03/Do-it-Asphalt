"""SM_ExitGate_Frame and SM_ExitGate_Barrier - the exit gate in the fence line.

SM_ExitGate_Frame (0.4 x 3.8 x 3.0 m; origin = centre of the footprint, on the ground):
  two yellow columns on concrete footings, 3.4 m apart (inner faces at Y = +-1.7),
  each with two orange warning bands, and a yellow box beam between their tops
  whose underside is at Z = 2.6. The middle of the beam is the white sign board
  for EXIT, 2.0 x 0.5 m, standing proud of BOTH faces of the beam (white faces
  at X = +-BOARD_HALF_X) so the game can write on either side. The board is
  0.5 m tall and the beam 0.4 m, so the board hangs 0.1 m below the beam: the
  passage is 3.4 m wide and 2.6 m high, 2.5 m under the board.
  Envelope: X +-0.2, Y +-1.9, Z [0, 3.0]. 5 materials.

SM_ExitGate_Barrier (0.10 x 3.4 x 0.12 m; origin = the hinge end, on the arm's
axis; the arm runs towards +Y, the game turns it about the X axis in the origin):
  chamfered arm in eight red / white sections, a dark hinge sleeve around the
  first of them and a dark cap on the free end.
  Envelope: X +-0.05, Y [0, 3.4], Z +-0.06. 3 materials.

Run:  blender -b -P Tools/ModelGen/exit_gate.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

BODY = "DIA_Yellow"
FRAME = "DIA_DarkSteel"

# --- frame ------------------------------------------------------------------
PASSAGE_HALF_WIDTH = 1.7             # inner faces of the columns
PASSAGE_HEIGHT = 2.6                 # underside of the beam
TOP = 3.0
COLUMN = 0.20                        # square section; outer faces at Y = +-1.9
FOOTING = (0.40, 0.20, 0.25)         # X x Y x Z; its X size is the envelope depth
BEAM_HALF_X = 0.08
BOARD_SIZE = (2.0, 0.5)              # Y x Z, docs/03 chapter 4.3
BOARD_HALF_X = 0.11                  # proud of the beam on both faces
BAND_Z = (0.50, 0.90)                # bottoms of the orange warning bands on the columns
BAND_H = 0.16
GUSSET = 0.34                        # dark gusset plates where beam and column meet
GUSSET_HALF_X = 0.11                 # proud of column (0.10) and beam on both faces
GUSSET_INSET = 0.02                  # kept below the top and inside the outer face

# --- arm --------------------------------------------------------------------
ARM_LENGTH = 3.4
ARM_SECTIONS = 8
ARM_HALF = (0.04, 0.05)              # half X, half Z of the arm itself
ARM_CHAMFER = 0.015
SLEEVE = (0.10, 0.14, 0.12)          # hinge sleeve: X and Z sizes are the envelope
ARM_START = SLEEVE[1] / 2            # the arm begins inside the sleeve
END_CAP = 0.03


def build_frame():
    name = "SM_ExitGate_Frame"
    c.reset_scene()
    parts = []

    column_y = PASSAGE_HALF_WIDTH + COLUMN / 2
    for side in (1, -1):
        y = side * column_y
        parts.append(c.box(FOOTING, (0, y, FOOTING[2] / 2), "DIA_Concrete"))
        # The column is a stack of sections, the warning bands being two of them, so
        # nothing stands into the 3.4 m passage.
        levels = [FOOTING[2]]
        for z in BAND_Z:
            levels += [z, z + BAND_H]
        levels.append(TOP)
        for i, (z0, z1) in enumerate(zip(levels, levels[1:])):
            parts.append(c.box((COLUMN, COLUMN, z1 - z0), (0, y, (z0 + z1) / 2),
                               "DIA_Orange" if i % 2 else BODY))
        # gusset plate on both faces of the corner (lies entirely over column and beam)
        outer = PASSAGE_HALF_WIDTH + COLUMN - GUSSET_INSET
        parts.append(c.box((2 * GUSSET_HALF_X, GUSSET, GUSSET),
                           (0, side * (outer - GUSSET / 2), TOP - GUSSET_INSET - GUSSET / 2), FRAME))

    # Beam: one half on each side of the sign board, which fills the middle.
    bw, bh = BOARD_SIZE
    beam_h = TOP - PASSAGE_HEIGHT
    for side in (1, -1):
        parts.append(c.box((2 * BEAM_HALF_X, PASSAGE_HALF_WIDTH - bw / 2, beam_h),
                           (0, side * (PASSAGE_HALF_WIDTH + bw / 2) / 2, PASSAGE_HEIGHT + beam_h / 2),
                           BODY))
    parts.append(c.box((2 * BOARD_HALF_X, bw, bh), (0, 0, TOP - bh / 2), "DIA_White"))

    return c.finish(parts, name)


def build_arm():
    name = "SM_ExitGate_Barrier"
    c.reset_scene()
    parts = []

    hx, hz = ARM_HALF
    k = ARM_CHAMFER
    # cross-section in the (z, x) order c.extrude() wants for a sweep along Y
    section = [(-hz + k, -hx), (hz - k, -hx), (hz, -hx + k), (hz, hx - k),
               (hz - k, hx), (-hz + k, hx), (-hz, hx - k), (-hz, -hx + k)]
    step = ARM_LENGTH / ARM_SECTIONS
    for i in range(ARM_SECTIONS):
        y0 = ARM_START if i == 0 else i * step
        y1 = ARM_LENGTH - END_CAP if i == ARM_SECTIONS - 1 else (i + 1) * step
        parts.append(c.extrude(section, y0, y1, "Y", ("DIA_Red", "DIA_White")[i % 2],
                               cap_lo=False, cap_hi=False))
    parts.append(c.box(SLEEVE, (0, SLEEVE[1] / 2, 0), FRAME))
    parts.append(c.extrude(section, ARM_LENGTH - END_CAP, ARM_LENGTH, "Y", FRAME, cap_lo=False))

    return c.finish(parts, name)


if __name__ == "__main__":
    build_frame()
    build_arm()
