"""Hotbar icons of the hand vessels: 256 x 256 PNG, transparent background.

Loads the GLBs from SourceAssets/Models (run their model scripts first) and
renders each one alone from a soft three-quarter front-top view with even
lighting, centred and filling ICON_FILL of the frame:

    SourceAssets/Icons/ICON_Bowl.png       SM_Bowl with SM_Spoon leaning in it
    SourceAssets/Icons/ICON_Bucket.png     SM_Bucket
    SourceAssets/Icons/ICON_Canister.png   SM_Canister
    SourceAssets/Icons/ICON_Tub.png        SM_Tub

Not a model script: the leading underscore makes build_all.sh skip it, and the
icons are not in manifest.json.

Run:  blender -b -P Tools/ModelGen/_icons.py
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402

ICONS_DIR = os.path.join(c.ROOT, "SourceAssets", "Icons")
ICON_SIZE = 256
ICON_FILL = 0.85
# Camera position relative to the subject, GAME axes (+X forward = the pouring side).
VIEW = (1.0, 0.75, 1.05)


def spoon_in_bowl():
    """Placement (game axes) of SM_Spoon standing in SM_Bowl: its bowl end on the
    floor, the handle leaning on the rim and sticking out across the view."""
    tilt = math.radians(42.0)                      # +X of the spoon (its bowl end) dips
    out = Vector((0.75, -0.66, 0.0)).normalized()  # where the handle points, seen from above
    heading = math.atan2(-out.y, -out.x)
    head_local = Vector((0.130, 0.0, -0.010))
    rest = out * 0.040 + Vector((0.0, 0.0, 0.022))  # where that point comes to lie
    return (Matrix.Translation(rest) @ Matrix.Rotation(heading, 4, "Z")
            @ Matrix.Rotation(tilt, 4, "Y") @ Matrix.Translation(-head_local))


ICONS = (  # icon name, [(model, placement in game axes or None)]
    ("ICON_Bowl", [("SM_Bowl", None), ("SM_Spoon", spoon_in_bowl())]),
    ("ICON_Bucket", [("SM_Bucket", None)]),
    ("ICON_Canister", [("SM_Canister", None)]),
    ("ICON_Tub", [("SM_Tub", None)]),
)

# Game axes <-> Blender axes of an imported GLB: Y is mirrored (see common.export).
FLIP = Matrix.Diagonal((1.0, -1.0, 1.0, 1.0))


def load(model, placement):
    path = os.path.join(c.MODELS_DIR, model + ".glb")
    if not os.path.isfile(path):
        raise FileNotFoundError("%s is missing - run its model script first" % path)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    objs = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    for o in objs:
        if placement is not None:
            o.matrix_world = FLIP @ placement @ FLIP @ o.matrix_world
        for poly in o.data.polygons:
            poly.use_smooth = False
    return objs


def setup_scene():
    c.reset_scene()
    scene = bpy.context.scene
    engine = c._set_engine(scene)
    render = scene.render
    render.resolution_x = render.resolution_y = ICON_SIZE
    render.resolution_percentage = 100
    render.film_transparent = True
    render.image_settings.file_format = "PNG"
    render.image_settings.color_mode = "RGBA"
    render.image_settings.compression = 100
    render.dither_intensity = 0.0
    scene.view_settings.view_transform = "Standard"
    try:
        scene.view_settings.look = "None"
    except TypeError:
        pass

    world = bpy.data.worlds.new("IconWorld")
    try:
        world.use_nodes = True
    except Exception:
        pass
    world.color = (0.85, 0.87, 0.90)
    if world.node_tree is not None:
        bg = next((n for n in world.node_tree.nodes if n.type == "BACKGROUND"), None)
        if bg is not None:
            bg.inputs["Color"].default_value = (0.85, 0.87, 0.90, 1.0)
            bg.inputs["Strength"].default_value = 0.55
    scene.world = world

    # Even light: a soft key from the front left whose shadow falls into the open
    # top (otherwise a vessel reads as closed with a lid), two shadowless fills; weak highlights so the steel does not blow out.
    for label, from_game, energy, shadow in (("Key", (0.35, -0.75, 0.65), 2.4, True),
                                             ("FillRight", (0.7, 0.9, 0.5), 0.8, False),
                                             ("FillBack", (-0.8, 0.6, 0.6), 0.5, False)):
        data = bpy.data.lights.new("Icon" + label, "SUN")
        data.energy = energy
        data.angle = math.radians(12.0)
        data.specular_factor = 0.25
        data.use_shadow = shadow
        light = bpy.data.objects.new("Icon" + label, data)
        light.matrix_world = c._look_matrix(Vector(), c._game_to_blender(from_game), (0, 0, 1))
        scene.collection.objects.link(light)

    cam_data = bpy.data.cameras.new("IconCamera")
    cam_data.type = "ORTHO"
    cam_data.clip_start = 0.01
    cam_data.clip_end = 100.0
    cam = bpy.data.objects.new("IconCamera", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return scene, cam, engine


def frame(cam, objs):
    """Orthographic fit: the subject centred, its larger screen extent = ICON_FILL."""
    back = c._game_to_blender(VIEW).normalized()
    cam.matrix_world = c._look_matrix(Vector(), back, (0, 0, 1))
    right, up = cam.matrix_world.col[0].xyz.copy(), cam.matrix_world.col[1].xyz.copy()
    pts = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    us = [p.dot(right) for p in pts]
    vs = [p.dot(up) for p in pts]
    cam.data.ortho_scale = max(max(us) - min(us), max(vs) - min(vs)) / ICON_FILL
    cam.location = (right * (max(us) + min(us)) / 2.0 + up * (max(vs) + min(vs)) / 2.0
                    + back * 10.0)


def main():
    os.makedirs(ICONS_DIR, exist_ok=True)
    for icon, models in ICONS:
        scene, cam, engine = setup_scene()
        objs = [o for model, placement in models for o in load(model, placement)]
        bpy.context.view_layer.update()
        frame(cam, objs)
        scene.render.filepath = os.path.join(ICONS_DIR, icon + ".png")
        try:
            bpy.ops.render.render(write_still=True)
        except RuntimeError as e:
            print("WARNING %s failed headless (%s), using Workbench" % (engine, e))
            c._setup_workbench(scene)
            scene.display.shading.show_shadows = False
            engine = scene.render.engine
            bpy.ops.render.render(write_still=True)
        print("ICON %s (%s)" % (scene.render.filepath, engine))


main()
