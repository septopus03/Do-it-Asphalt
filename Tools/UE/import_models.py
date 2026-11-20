"""Imports every SourceAssets/Models/*.glb as a static mesh /Game/Models/<Name> (Interchange glTF).

Run: Scripts/uepy.sh Tools/UE/import_models.py
Prints `DIA_OK /Game/Models/<Name> <sizeX> <sizeY> <sizeZ>` per mesh (bounding box in cm); on error prints
`DIA_FAIL ...` and exits non-zero.

Result per model (see docs/03-3d-modely.md):
  - static mesh /Game/Models/<Name>, 1 unit = 1 cm, game axes (+X forward, +Y right, +Z up);
  - every material slot, named after the GLB material (e.g. DIA_Yellow), assigned to /Game/Materials/MI_<slot name>;
  - simple convex collision (18-DOP), except SM_Mall: one box over its envelope (BOX_COLLISION_MODELS); no lightmap UV generation, no Nanite, no extra assets in /Game/Models
    (materials/textures Interchange creates are deleted again, last; any other unexpected asset fails the run).

Idempotent: an existing mesh is re-imported in place (same asset path, so references to it survive), the
slots are assigned again, and stray assets in /Game/Models are removed - a second run gives the same output.

Needs Tools/ModelGen/common.py glTF(x, y, z) = game(x, z, y); the import options below are the conversion.
"""
import glob
import os
import sys
import traceback

import unreal

MODELS_DIR = "/Game/Models"
MATERIAL_DIR = "/Game/Materials"
asset_lib = unreal.EditorAssetLibrary
# Models whose collision is one box over the envelope instead of the convex hull (the mall is a 100 x 100 m block).
BOX_COLLISION_MODELS = {"SM_Mall"}


def source_files():
    root = unreal.Paths.project_dir()
    pattern = os.path.join(root, "SourceAssets", "Models", "*.glb")
    files = sorted(glob.glob(pattern))
    if not files:
        raise RuntimeError("no .glb files in %s" % pattern)
    return files


def make_pipeline():
    """Interchange pipeline: static meshes only, palette materials are assigned afterwards by name."""
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property("pipeline_display_name", "DIA_Models")
    pipeline.set_editor_property("use_source_name_for_asset", True)
    pipeline.set_editor_property("asset_type_sub_folders", False)
    pipeline.set_editor_property("scene_name_sub_folder", False)

    # Meshes: static only, no combining, simple convex collision, no lightmap UVs.
    common = pipeline.get_editor_property("common_meshes_properties")
    common.set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    common.set_editor_property("bake_meshes", True)
    common.set_editor_property("import_lods", False)

    mesh = pipeline.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_static_meshes", True)
    mesh.set_editor_property("import_skeletal_meshes", False)
    mesh.set_editor_property("combine_static_meshes_behavior",
                             unreal.InterchangeCombineStaticMeshesBehavior.ALL)
    mesh.set_editor_property("collision", True)
    mesh.set_editor_property("fallback_collision_type", unreal.InterchangeMeshCollision.CONVEX18DOP)
    mesh.set_editor_property("generate_lightmap_u_vs", False)
    mesh.set_editor_property("build_nanite", False)

    # Materials/textures: not needed, the slots are pointed at the existing palette instances afterwards.
    materials = pipeline.get_editor_property("material_pipeline")
    materials.set_editor_property("import_materials", False)
    return pipeline


def import_one(glb_path, pipeline):
    name = os.path.splitext(os.path.basename(glb_path))[0]
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", glb_path)
    task.set_editor_property("destination_path", MODELS_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", False)
    override = unreal.InterchangePipelineStackOverride()
    override.add_pipeline(pipeline)
    task.set_editor_property("options", override)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    # import_asset_tasks does not raise when Interchange fails to parse a file; on a re-run the OLD mesh would
    # still sit at the path and pass every later check. So the task itself must report the imported mesh.
    path = "%s/%s" % (MODELS_DIR, name)
    reported = [str(p).split(".")[0] for p in task.get_editor_property("imported_object_paths")]
    if path not in reported:
        raise RuntimeError("import of %s did not report %s (imported: %s)" % (glb_path, path, reported))
    mesh = asset_lib.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("import of %s did not create static mesh %s (got %s)" % (glb_path, path, mesh))
    return name, path, mesh


def assign_palette(path, mesh):
    slots = mesh.get_editor_property("static_materials")
    if not slots:
        raise RuntimeError("%s has no material slots" % path)
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        target = "%s/MI_%s" % (MATERIAL_DIR, slot_name)
        material = asset_lib.load_asset(target) if asset_lib.does_asset_exist(target) else None
        if material is None:
            raise RuntimeError("%s: slot '%s' has no palette material %s" % (path, slot_name, target))
        mesh.set_material(index, material)


def apply_box_collision(name, mesh):
    """Replaces the generated collision of a BOX_COLLISION_MODELS model by one box over the bounding box."""
    if name not in BOX_COLLISION_MODELS:
        return
    body_setup = mesh.get_editor_property("body_setup")
    geom = body_setup.get_editor_property("agg_geom")
    box = mesh.get_bounding_box()
    size = box.max - box.min
    elem = unreal.KBoxElem()
    elem.set_editor_property("center", (box.max + box.min) * 0.5)
    elem.set_editor_property("x", size.x)
    elem.set_editor_property("y", size.y)
    elem.set_editor_property("z", size.z)
    geom.set_editor_property("convex_elems", [])
    geom.set_editor_property("box_elems", [elem])
    body_setup.set_editor_property("agg_geom", geom)
    mesh.modify()


def check_collision(path, mesh):
    geom = mesh.get_editor_property("body_setup").get_editor_property("agg_geom")
    convex = len(geom.get_editor_property("convex_elems"))
    print("DIA_INFO %s collision convex=%d box=%d sphere=%d" % (
        path, convex, len(geom.get_editor_property("box_elems")), len(geom.get_editor_property("sphere_elems"))))
    boxes = len(geom.get_editor_property("box_elems"))
    if path.rsplit("/", 1)[-1] in BOX_COLLISION_MODELS:
        if boxes != 1 or convex != 0:
            raise RuntimeError("%s needs exactly one box collision (got %d box, %d convex)" % (path, boxes, convex))
    elif convex < 1:
        raise RuntimeError("%s has no convex collision" % path)


def bounds_cm(mesh):
    box = mesh.get_bounding_box()
    return box.max - box.min, box


def signed_volume(mesh):
    """Signed volume of LOD 0 in cm3 and its triangle count. The sign encodes the winding: UE's own closed
    shapes are negative, a positive value means inside-out faces (mirror without a winding flip)."""
    desc = mesh.get_static_mesh_description(0)
    total = 0.0
    count = desc.get_triangle_count()
    for t in range(count):
        tri = unreal.TriangleID()
        tri.set_editor_property("id_value", t)
        # The Python wrapper returns the three vertex ids as the last items of the returned array.
        ids = list(desc.get_triangle_vertices(tri))[-3:]
        a, b, c = [desc.get_vertex_position(v) for v in ids]
        total += (a.x * (b.y * c.z - b.z * c.y) + a.y * (b.z * c.x - b.x * c.z) + a.z * (b.x * c.y - b.y * c.x)) / 6.0
    return total, count


def check_winding(path, mesh, reference):
    volume, triangles = signed_volume(mesh)
    if volume * reference <= 0:
        raise RuntimeError("%s has inside-out faces (signed volume %.0f cm3, engine cube %.0f)" % (path, volume, reference))
    return triangles


def plan_cleanup(keep_paths):
    """Returns the stray assets to delete: only Material*/Texture* assets Interchange may have created.
    Any other asset in /Game/Models (a mesh without a .glb, another class) raises instead of being deleted."""
    strays = []
    for asset_path in asset_lib.list_assets(MODELS_DIR, recursive=True, include_folder=False):
        clean = asset_path.split(".")[0]
        if clean in keep_paths:
            continue
        asset = asset_lib.load_asset(clean)
        if isinstance(asset, (unreal.MaterialInterface, unreal.Texture)):
            strays.append(clean)
        else:
            raise RuntimeError("unexpected asset %s (%s) in %s: no matching .glb or not a material/texture; "
                               "remove or move it by hand" % (clean, type(asset).__name__, MODELS_DIR))
    return strays


def remove_strays(strays):
    """Deletes the Interchange materials/textures and the sub-folders (Materials/, Textures/) left empty."""
    for clean in strays:
        if not asset_lib.delete_asset(clean):
            raise RuntimeError("could not delete stray asset %s" % clean)
        print("DIA_INFO removed stray asset %s" % clean)
    for folder in sorted(asset_lib.list_assets(MODELS_DIR, recursive=True, include_folder=True), reverse=True):
        folder = folder.rstrip("/")
        if folder != MODELS_DIR and asset_lib.does_directory_exist(folder) and not asset_lib.does_directory_have_assets(folder, recursive=True):
            asset_lib.delete_directory(folder)
            print("DIA_INFO removed empty folder %s" % folder)


def build():
    meshes = []
    for glb in source_files():
        pipeline = make_pipeline()
        name, path, mesh = import_one(glb, pipeline)
        meshes.append((name, path, mesh))

    # Everything is checked and saved first; the cleanup is last so a failure cannot leave a half-cleaned state.
    strays = plan_cleanup(set(path for _, path, _ in meshes))
    reference_volume, _ = signed_volume(asset_lib.load_asset("/Engine/BasicShapes/Cube"))
    results = []
    for name, path, mesh in meshes:
        assign_palette(path, mesh)
        apply_box_collision(name, mesh)
        check_collision(path, mesh)
        triangles = check_winding(path, mesh, reference_volume)
        if not asset_lib.save_asset(path, only_if_is_dirty=False):
            raise RuntimeError("could not save %s" % path)
        size, box = bounds_cm(mesh)
        results.append((path, size, box, triangles))

    remove_strays(strays)

    for path, size, box, triangles in results:
        print("DIA_INFO %s bounds min=(%.2f, %.2f, %.2f) max=(%.2f, %.2f, %.2f) triangles=%d" % (
            path, box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, triangles))
        print("DIA_OK %s %.1f %.1f %.1f" % (path, size.x, size.y, size.z))


try:
    build()
except Exception:
    traceback.print_exc()
    print("DIA_FAIL import_models: %s" % sys.exc_info()[1])
    sys.stdout.flush()
    os._exit(1)
