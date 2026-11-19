#!/usr/bin/env python3
"""Check generated GLB models against manifest.json. Runs without Blender.

Usage:  python3 Tools/ModelGen/check.py [Name ...]
        (no arguments = every model in the manifest; a missing file is a failure)

Prints `OK <Name>` or `FAIL <Name>: <reason>` per model; exit code 1 if any failed.

Checked per model, in game axes (+X forward, +Y right, +Z up, metres):
  - envelope min/max against the manifest, within its tolerance
  - triangle count <= max_tris
  - exactly one mesh, on a node without any transform
  - only palette materials, at most MAX_MATERIALS, with the palette's values
  - positive signed volume (faces wound outward; Unreal's import rejects inside-out meshes)
"""
import json
import os
import struct
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from common import MAX_MATERIALS, PALETTE, hex_to_linear  # noqa: E402  (bpy-free part)

MODELS_DIR = os.path.join(ROOT, "SourceAssets", "Models")
MANIFEST = os.path.join(HERE, "manifest.json")
COLOUR_EPS = 0.005
AXES = "xyz"


class CheckError(Exception):
    pass


def gltf_to_game(v):
    """glTF (+Y up, right-handed) -> game (+Z up, left-handed): swap Y and Z.

    Inverse of common.export(), and the same mapping Unreal's glTF importer applies
    (besides its metres -> centimetres scale).
    """
    return (v[0], v[2], v[1])


def read_glb(path):
    """Return (json dict, binary chunk bytes or b'')."""
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 20 or data[:4] != b"glTF":
        raise CheckError("not a GLB file")
    version, length = struct.unpack_from("<II", data, 4)
    if version != 2:
        raise CheckError("GLB version %d, expected 2" % version)
    if length != len(data):
        raise CheckError("GLB length field %d does not match file size %d" % (length, len(data)))
    offset = 12
    gltf, binary = None, b""
    while offset + 8 <= len(data):
        chunk_len, chunk_type = struct.unpack_from("<II", data, offset)
        body = data[offset + 8:offset + 8 + chunk_len]
        if chunk_type == 0x4E4F534A and gltf is None:  # 'JSON'
            gltf = json.loads(body.decode("utf-8"))
        elif chunk_type == 0x004E4942 and not binary:  # 'BIN'
            binary = body
        offset += 8 + chunk_len
    if gltf is None:
        raise CheckError("GLB has no JSON chunk")
    return gltf, binary


def read_glb_json(path):
    return read_glb(path)[0]


_COMPONENT = {5121: "B", 5123: "H", 5125: "I", 5126: "f"}
_WIDTH = {"SCALAR": 1, "VEC3": 3}


def read_accessor(gltf, binary, index):
    """Accessor -> list of tuples (VEC3) or plain numbers (SCALAR)."""
    acc = gltf["accessors"][index]
    view = gltf["bufferViews"][acc["bufferView"]]
    fmt = _COMPONENT[acc["componentType"]]
    width = _WIDTH[acc["type"]]
    size = struct.calcsize("<" + fmt * width)
    stride = view.get("byteStride", size)
    start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    out = []
    for i in range(acc["count"]):
        item = struct.unpack_from("<" + fmt * width, binary, start + i * stride)
        out.append(item if width > 1 else item[0])
    return out


def signed_volume(gltf, binary):
    """Signed volume of the mesh in m^3 (glTF is right-handed, front faces counter-clockwise):
    positive when the faces point outward."""
    total = 0.0
    for prim in gltf["meshes"][0].get("primitives", []):
        pos = read_accessor(gltf, binary, prim["attributes"]["POSITION"])
        idx = read_accessor(gltf, binary, prim["indices"]) if "indices" in prim else range(len(pos))
        idx = list(idx)
        for t in range(0, len(idx) - 2, 3):
            a, b, c = pos[idx[t]], pos[idx[t + 1]], pos[idx[t + 2]]
            total += (a[0] * (b[1] * c[2] - b[2] * c[1])
                      - a[1] * (b[0] * c[2] - b[2] * c[0])
                      + a[2] * (b[0] * c[1] - b[1] * c[0]))
    return total / 6.0


def check_model(entry):
    """Return a list of failure reasons (empty = OK)."""
    path = os.path.join(MODELS_DIR, entry["name"] + ".glb")
    if not os.path.isfile(path):
        return ["file missing"]
    try:
        gltf, binary = read_glb(path)
    except (CheckError, ValueError, struct.error) as e:
        return [str(e)]

    problems = []
    meshes = gltf.get("meshes", [])
    accessors = gltf.get("accessors", [])
    materials = gltf.get("materials", [])
    if len(meshes) != 1:
        return ["%d meshes, expected exactly 1" % len(meshes)]

    mesh_nodes = [n for n in gltf.get("nodes", []) if "mesh" in n]
    if len(mesh_nodes) != 1:
        problems.append("%d nodes use the mesh, expected 1" % len(mesh_nodes))
    for n in gltf.get("nodes", []):
        if any(k in n for k in ("matrix", "translation", "rotation", "scale")):
            problems.append("node '%s' has a transform (transforms must be applied)" % n.get("name", "?"))

    # Envelope and triangles.
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    tris = 0
    used_materials = set()
    for prim in meshes[0].get("primitives", []):
        if prim.get("mode", 4) != 4:
            return ["primitive mode %d, expected triangles (4)" % prim["mode"]]
        pos = accessors[prim["attributes"]["POSITION"]]
        if "min" not in pos or "max" not in pos:
            return ["POSITION accessor without min/max"]
        for i in range(3):
            lo[i] = min(lo[i], pos["min"][i])
            hi[i] = max(hi[i], pos["max"][i])
        count = accessors[prim["indices"]]["count"] if "indices" in prim else pos["count"]
        tris += count // 3
        if "material" in prim:
            used_materials.add(prim["material"])
        else:
            problems.append("primitive without material")
    if tris == 0:
        return ["no triangles"]

    lo, hi = gltf_to_game(lo), gltf_to_game(hi)
    tol = entry.get("tolerance", 0.02)
    for label, got, want in (("min", lo, entry["min"]), ("max", hi, entry["max"])):
        for i in range(3):
            if abs(got[i] - want[i]) > tol + 1e-6:
                problems.append("envelope %s %s is %.3f, expected %.3f +-%.3f"
                                % (label, AXES[i], got[i], want[i], tol))

    if tris > entry["max_tris"]:
        problems.append("%d triangles, limit %d" % (tris, entry["max_tris"]))

    try:
        volume = signed_volume(gltf, binary)
    except (KeyError, IndexError, struct.error) as e:
        return problems + ["cannot read the geometry (%s)" % e]
    if volume <= 0.0:
        problems.append("signed volume %.6f m3 is not positive (faces wound inside-out)" % volume)

    # Materials.
    if len(materials) > MAX_MATERIALS:
        problems.append("%d materials, limit %d" % (len(materials), MAX_MATERIALS))
    if len(used_materials) != len(materials):
        problems.append("file contains unused materials")
    for m in materials:
        name = m.get("name", "")
        if name not in PALETTE:
            problems.append("material '%s' is not in the palette" % name)
            continue
        hex_colour, metallic, roughness = PALETTE[name]
        pbr = m.get("pbrMetallicRoughness", {})
        got = (list(pbr.get("baseColorFactor", [1, 1, 1, 1]))[:3]
               + [pbr.get("metallicFactor", 1.0), pbr.get("roughnessFactor", 1.0)])
        want = list(hex_to_linear(hex_colour)) + [metallic, roughness]
        if any(abs(g - w) > COLOUR_EPS for g, w in zip(got, want)):
            problems.append("material '%s' values differ from the palette" % name)
        if any(k in pbr for k in ("baseColorTexture", "metallicRoughnessTexture")) or gltf.get("images"):
            problems.append("material '%s' uses textures" % name)
    return problems


def main(argv):
    with open(MANIFEST, encoding="utf-8") as f:
        manifest = {e["name"]: e for e in json.load(f)}
    names = argv or list(manifest)
    failed = False
    for name in names:
        problems = check_model(manifest[name]) if name in manifest else ["not in manifest.json"]
        if problems:
            failed = True
            print("FAIL %s: %s" % (name, "; ".join(problems)))
        else:
            print("OK %s" % name)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
