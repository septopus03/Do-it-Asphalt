"""Creates /Game/Materials: M_Ground, M_DIA_Flat and the 12 MI_DIA_* palette instances.

Run: Scripts/uepy.sh Tools/UE/create_materials.py
Prints `DIA_OK <asset path>` per saved asset (14 in total); on error prints `DIA_FAIL ...`
and exits non-zero. Idempotent: existing assets are cleared/updated in place.

The palette must match docs/03-3d-modely.md chapter 3 and Tools/ModelGen/common.py PALETTE
(duplicated here on purpose: this script runs inside Unreal and must not import Blender code).
"""
import os
import sys
import traceback

import unreal

MATERIAL_DIR = "/Game/Materials"

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


def build_ground():
    path, mat = get_or_create("M_Ground", unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)

    # Engine black texture as default: no coverage, no lines (plain dirt). It is sRGB, so the
    # sampler type must be Color or the material fails to compile; the textures that replace it
    # at runtime are plain mask data and are sampled by their own (non-sRGB) resource format.
    black = unreal.EditorAssetLibrary.load_asset("/Engine/EngineResources/Black")

    coverage = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1400, 0,
                    parameter_name="CoverageTex", texture=black,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    hexlines = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1400, -400,
                    parameter_name="HexLinesTex", texture=black,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
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

    mel.recompile_material(mat)
    save(path)


def build_flat():
    path, mat = get_or_create("M_DIA_Flat", unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)

    colour = node(mat, unreal.MaterialExpressionVectorParameter, -600, 0,
                  parameter_name="BaseColor", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    metallic = node(mat, unreal.MaterialExpressionScalarParameter, -600, 200,
                    parameter_name="Metallic", default_value=0.0)
    roughness = node(mat, unreal.MaterialExpressionScalarParameter, -600, 300,
                     parameter_name="Roughness", default_value=0.5)
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    save(path)
    return mat


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


try:
    build()
except Exception:
    traceback.print_exc()
    print("DIA_FAIL create_materials: %s" % sys.exc_info()[1])
    sys.stdout.flush()
    os._exit(1)
