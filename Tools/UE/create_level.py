"""Creates /Game/Maps/L_Main: sun, sky atmosphere, sky light, height fog, PlayerStart.

Run: Scripts/uepy.sh Tools/UE/create_level.py
No ground on purpose - Task 9 builds it. Idempotent: an existing map is loaded, cleared and rebuilt.
Prints `DIA_OK <asset path>` per saved asset; on error prints `DIA_FAIL ...` and exits non-zero.
"""
import os
import sys
import traceback

import unreal

MAP_PATH = "/Game/Maps/L_Main"


def spawn(actor_class, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), label=None):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = subsystem.spawn_actor_from_class(
        actor_class, unreal.Vector(*location), unreal.Rotator(roll=rotation[0], pitch=rotation[1], yaw=rotation[2])
    )
    if actor is None:
        raise RuntimeError("could not spawn %s" % actor_class)
    if label:
        actor.set_actor_label(label)
    return actor


def build():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        # new_level refuses an existing map (and delete_asset leaves the file), so reuse it:
        # load it and clear everything but the built-in actors, then rebuild below.
        if not level_subsystem.load_level(MAP_PATH):
            raise RuntimeError("could not load existing %s" % MAP_PATH)
        for actor in actor_subsystem.get_all_level_actors():
            if not isinstance(actor, (unreal.Brush, unreal.WorldSettings)):
                actor_subsystem.destroy_actor(actor)
    elif not level_subsystem.new_level(MAP_PATH):
        raise RuntimeError("new_level failed for %s" % MAP_PATH)

    sun = spawn(unreal.DirectionalLight, rotation=(0.0, -50.0, 30.0), label="Sun")
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 10.0)
    spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
    spawn(unreal.SkyLight, location=(0.0, 0.0, 500.0), label="SkyLight")
    spawn(unreal.ExponentialHeightFog, location=(0.0, 0.0, 0.0), label="HeightFog")
    spawn(unreal.PlayerStart, location=(-1600.0, 0.0, 100.0), label="PlayerStart")

    if not level_subsystem.save_current_level():
        raise RuntimeError("save_current_level failed")
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("%s not found after save" % MAP_PATH)
    print("DIA_OK %s" % MAP_PATH)


try:
    build()
except Exception:
    traceback.print_exc()
    print("DIA_FAIL create_level: %s" % sys.exc_info()[1])
    sys.stdout.flush()
    os._exit(1)
