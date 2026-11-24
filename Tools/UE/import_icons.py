"""Imports every SourceAssets/Icons/ICON_*.png as a UI texture /Game/Icons/ICON_<Name> (made by Tools/ModelGen/_icons.py).

Run: Scripts/uepy.sh Tools/UE/import_icons.py   (the editor must not be running)
Prints `DIA_OK /Game/Icons/ICON_<Name> <width> <height>` per icon; on error prints `DIA_FAIL ...` and exits non-zero.

Idempotent: an existing texture is replaced in place. The icons are drawn in the HUD and in the shop, so they are UI textures:
no mip maps, never streamed, sRGB with alpha.
"""
import glob
import os
import sys
import traceback

import unreal

ICON_DIR = "/Game/Icons"


def main():
    root = unreal.Paths.project_dir()
    files = sorted(glob.glob(os.path.join(root, "SourceAssets", "Icons", "ICON_*.png")))
    if not files:
        raise RuntimeError("no ICON_*.png files in SourceAssets/Icons")

    tasks = []
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0]
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", ICON_DIR)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("replace_existing_settings", True)
        task.set_editor_property("save", False)
        task.set_editor_property("factory", unreal.TextureFactory())
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    failed = False
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0]
        asset_path = "%s/%s" % (ICON_DIR, name)
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        if texture is None:
            print("DIA_FAIL missing %s" % asset_path)
            failed = True
            continue
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        unreal.EditorAssetLibrary.save_asset(asset_path)
        print("DIA_OK %s %d %d" % (asset_path, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
    if failed:
        sys.exit(1)


try:
    main()
except Exception:
    traceback.print_exc()
    print("DIA_FAIL exception")
    sys.exit(1)
