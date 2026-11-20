"""Creates /Game/Materials: M_Ground, M_DIA_Flat and the 12 MI_DIA_* palette instances.

Run: Scripts/uepy.sh Tools/UE/create_materials.py
Prints `DIA_OK <asset path>` per saved asset (14 in total); on error prints `DIA_FAIL ...`
and exits non-zero. Idempotent: existing assets are cleared/updated in place.

The palette must match docs/03-3d-modely.md chapter 3 and Tools/ModelGen/common.py PALETTE
(duplicated here on purpose: this script runs inside Unreal and must not import Blender code).
"""
import os
import struct
import sys
import tempfile
import traceback
import zlib

import unreal

MATERIAL_DIR = "/Game/Materials"

# Factor on the (linear) letter colour in M_DIA_Text, see build_text.
TEXT_BRIGHTNESS = 0.5

# name -> (hex sRGB colour, metallic, roughness)
PALETTE = {
    "DIA_Yellow":    ("#F2B705", 0.0, 0.6),
    "DIA_Orange":    ("#E8590C", 0.0, 0.6),
    "DIA_Steel":     ("#8A9099", 1.0, 0.4),
    "DIA_DarkSteel": ("#3A3F47", 1.0, 0.5),
    "DIA_Rubber":    ("#1B1B1D", 0.0, 0.9),
    "DIA_Asphalt":   ("#232326", 0.0, 0.35),
    "DIA_White":     ("#F2F2EE", 0.0, 0.7),
    "DIA_Red":       ("#C62828", 0.0, 0.6),
    "DIA_Glass":     ("#7FB6D9", 0.0, 0.1),
    "DIA_Concrete":  ("#A8A49C", 0.0, 0.9),
    "DIA_Dirt":      ("#8A6B4A", 0.0, 1.0),
    "DIA_Green":     ("#3E8E41", 0.0, 0.6),
}

mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def hex_to_linear(hex_colour):
    """'#RRGGBB' (sRGB) -> unreal.LinearColor with linear components (alpha 1)."""
    h = hex_colour.lstrip("#")
    out = []
    for i in (0, 2, 4):
        c = int(h[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return unreal.LinearColor(out[0], out[1], out[2], 1.0)


def get_or_create(name, asset_class, factory):
    path = "%s/%s" % (MATERIAL_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
    else:
        asset = asset_tools.create_asset(name, MATERIAL_DIR, asset_class, factory)
    if asset is None:
        raise RuntimeError("could not create/load %s" % path)
    return path, asset


def save(path):
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError("could not save %s" % path)
    print("DIA_OK %s" % path)


def node(material, cls, x, y, **props):
    expr = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr


def link(src, dst, dst_input, src_output=""):
    if not mel.connect_material_expressions(src, src_output, dst, dst_input):
        raise RuntimeError("could not connect %s -> %s.%s" % (src.get_name(), dst.get_name(), dst_input))


def custom(material, x, y, description, code, input_names):
    expr = node(material, unreal.MaterialExpressionCustom, x, y,
                description=description, code=code, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = []
    for n in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    expr.set_editor_property("inputs", inputs)
    return expr


def ensure_black_mask():
    """Tiny black linear mask texture used as the default of CoverageTex / HexLinesTex.

    Saved as /Game/Materials/T_Mask_Black (a support asset: reported as DIA_AUX, not DIA_OK, so the
    script keeps its 14 DIA_OK lines = 2 materials + 12 instances).
    """
    name = "T_Mask_Black"
    path = "%s/%s" % (MATERIAL_DIR, name)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        size = 4
        raw = b"".join(b"\x00" + b"\x00" * size for _ in range(size))  # filter byte + gray row
        def chunk(tag, data):
            body = tag + data
            return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
        png = (b"\x89PNG\r\n\x1a\n"
               + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 0, 0, 0, 0))
               + chunk(b"IDAT", zlib.compress(raw))
               + chunk(b"IEND", b""))
        tmp_dir = tempfile.mkdtemp(prefix="dia_tex_")
        png_path = os.path.join(tmp_dir, name + ".png")
        with open(png_path, "wb") as f:
            f.write(png)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", png_path)
        task.set_editor_property("destination_path", MATERIAL_DIR)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        task.set_editor_property("factory", unreal.TextureFactory())
        asset_tools.import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset(path)
    if tex is None:
        raise RuntimeError("could not import %s" % path)
    # (Re)apply the contract every run so a re-run also repairs a hand-edited texture.
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError("could not save %s" % path)
    print("DIA_AUX %s" % path)
    return tex


def expect_nodes(material, count):
    got = mel.get_num_material_expressions(material)
    if got != count:
        raise RuntimeError("%s has %d expressions, expected %d" % (material.get_name(), got, count))


def clear_expressions(material):
    """Remove every node so a re-run rebuilds the graph from scratch.

    delete_all_material_expressions alone left stale nodes behind across runs (28 nodes after a few
    runs instead of 15), so delete them explicitly and check that nothing is left.
    """
    mel.delete_all_material_expressions(material)
    for expr in list(mel.get_material_expressions(material)):
        mel.delete_material_expression(material, expr)
    left = mel.get_num_material_expressions(material)
    if left:
        raise RuntimeError("%s still has %d expressions after clearing" % (material.get_name(), left))


def build_ground():
    path, mat = get_or_create("M_Ground", unreal.Material, unreal.MaterialFactoryNew())
    clear_expressions(mat)

    # TEXTURE CONTRACT (Task 9): CoverageTex and HexLinesTex are plain linear mask data read from
    # the R channel, so any texture assigned to them must be SRGB=false (otherwise the sRGB curve
    # turns coverage 0.5 into ~0.21 and shifts the smoothstep centre and the > 0.5 hex threshold),
    # compression TC_Masks / linear, and clamp addressing (so the 0..1 UV edges do not wrap).
    # Sampler type is Masks to match; the defaults are a generated black non-sRGB mask texture
    # (the engine's own Black texture is sRGB and fails the sampler check).
    black = ensure_black_mask()

    coverage = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1400, 0,
                    parameter_name="CoverageTex", texture=black,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    hexlines = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1400, -400,
                    parameter_name="HexLinesTex", texture=black,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    softness = node(mat, unreal.MaterialExpressionScalarParameter, -1400, 400,
                    parameter_name="EdgeSoftness", default_value=0.1)

    # Hex lines mask: 1 where HexLinesTex (R) > 0.5, else 0.
    hex_mask = custom(mat, -1000, -400, "HexMask", "return (HexLines > 0.5) ? 1.0 : 0.0;", ["HexLines"])
    link(hexlines, hex_mask, "HexLines", "R")
    # Asphalt weight: smoothstep(0.5 - EdgeSoftness, 0.5 + EdgeSoftness, CoverageTex.R).
    # The softness is clamped so EdgeSoftness = 0 gives a hard edge instead of a 0/0.
    weight = custom(mat, -1000, 100, "AsphaltWeight",
                    "float e = max(Softness, 0.0001); return smoothstep(0.5 - e, 0.5 + e, Coverage);",
                    ["Coverage", "Softness"])
    link(coverage, weight, "Coverage", "R")
    link(softness, weight, "Softness")

    dirt = node(mat, unreal.MaterialExpressionConstant3Vector, -1000, -700, constant=hex_to_linear("#8A6B4A"))
    white = node(mat, unreal.MaterialExpressionConstant3Vector, -1000, -900, constant=hex_to_linear("#F2F2EE"))
    asphalt = node(mat, unreal.MaterialExpressionConstant3Vector, -1000, -1100, constant=hex_to_linear("#232326"))

    base_dirt_white = node(mat, unreal.MaterialExpressionLinearInterpolate, -600, -500)
    link(dirt, base_dirt_white, "A")
    link(white, base_dirt_white, "B")
    link(hex_mask, base_dirt_white, "Alpha")
    base = node(mat, unreal.MaterialExpressionLinearInterpolate, -300, -300)
    link(base_dirt_white, base, "A")
    link(asphalt, base, "B")
    link(weight, base, "Alpha")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Roughness follows the palette: dirt 1.0, white marking 0.7, asphalt 0.35.
    rough_dirt = node(mat, unreal.MaterialExpressionConstant, -1000, 300, r=1.0)
    rough_white = node(mat, unreal.MaterialExpressionConstant, -1000, 400, r=0.7)
    rough_asphalt = node(mat, unreal.MaterialExpressionConstant, -1000, 500, r=0.35)
    rough_dw = node(mat, unreal.MaterialExpressionLinearInterpolate, -600, 300)
    link(rough_dirt, rough_dw, "A")
    link(rough_white, rough_dw, "B")
    link(hex_mask, rough_dw, "Alpha")
    rough = node(mat, unreal.MaterialExpressionLinearInterpolate, -300, 300)
    link(rough_dw, rough, "A")
    link(rough_asphalt, rough, "B")
    link(weight, rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    expect_nodes(mat, 15)
    mel.recompile_material(mat)
    save(path)


def build_flat():
    path, mat = get_or_create("M_DIA_Flat", unreal.Material, unreal.MaterialFactoryNew())
    clear_expressions(mat)

    colour = node(mat, unreal.MaterialExpressionVectorParameter, -600, 0,
                  parameter_name="BaseColor", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    metallic = node(mat, unreal.MaterialExpressionScalarParameter, -600, 200,
                    parameter_name="Metallic", default_value=0.0)
    roughness = node(mat, unreal.MaterialExpressionScalarParameter, -600, 300,
                     parameter_name="Roughness", default_value=0.5)
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    expect_nodes(mat, 3)
    mel.recompile_material(mat)
    save(path)
    return mat


def build_text():
    """/Game/Materials/M_DIA_Text: the text material for UTextRenderComponent (kiosk boards, exit sign), unlit.

    The engine's DefaultTextMaterialOpaque is a lit material: its specular term adds a grey haze, so black letters
    come out mid-grey and dark red ones pale pink in the sun. This copy keeps its wiring (Font parameter, masked,
    two-sided, vertex colour from the component's text colour) but is Unlit with the vertex colour as emissive, so the
    letters show the colour they were given. Reported as DIA_AUX (a support asset), like T_Mask_Black.
    """
    src = "/Engine/EngineMaterials/DefaultTextMaterialOpaque"
    path = "%s/M_DIA_Text" % MATERIAL_DIR
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if unreal.EditorAssetLibrary.duplicate_asset(src, path) is None:
            raise RuntimeError("could not duplicate %s to %s" % (src, path))
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if mat is None:
        raise RuntimeError("could not load %s" % path)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    vertex_colour = None
    for expr in mel.get_material_expressions(mat):
        if isinstance(expr, unreal.MaterialExpressionVertexColor):
            vertex_colour = expr
    if vertex_colour is None:
        raise RuntimeError("%s has no vertex colour node" % path)
    # Emissive is measured in the same units as the lit scene, so an unscaled colour is far brighter than a sun-lit white
    # board; the factor brings the letters down to the colour that was asked for (tuned by looking at daylight screenshots).
    # Reuse the nodes of an earlier run (deleting expressions of an engine copy crashed the editor once), else create them.
    existing = {type(e): e for e in mel.get_material_expressions(mat)}
    brightness = existing.get(unreal.MaterialExpressionScalarParameter) or node(
        mat, unreal.MaterialExpressionScalarParameter, -500, 200, parameter_name="TextBrightness", default_value=TEXT_BRIGHTNESS)
    brightness.set_editor_property("default_value", TEXT_BRIGHTNESS)
    scaled = existing.get(unreal.MaterialExpressionMultiply) or node(mat, unreal.MaterialExpressionMultiply, -300, 100)
    link(vertex_colour, scaled, "A")
    link(brightness, scaled, "B")
    mel.connect_material_property(scaled, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError("could not save %s" % path)
    print("DIA_AUX %s" % path)


def build_instances(parent):
    for name, (hex_colour, metallic, roughness) in PALETTE.items():
        inst_name = "MI_" + name
        path, inst = get_or_create(inst_name, unreal.MaterialInstanceConstant,
                                   unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(inst, parent)
        mel.set_material_instance_vector_parameter_value(inst, "BaseColor", hex_to_linear(hex_colour))
        mel.set_material_instance_scalar_parameter_value(inst, "Metallic", metallic)
        mel.set_material_instance_scalar_parameter_value(inst, "Roughness", roughness)
        mel.update_material_instance(inst)
        save(path)


def build():
    build_ground()
    parent = build_flat()
    build_instances(parent)
    build_text()


try:
    build()
except Exception:
    traceback.print_exc()
    print("DIA_FAIL create_materials: %s" % sys.exc_info()[1])
    sys.stdout.flush()
    os._exit(1)
