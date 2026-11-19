#!/usr/bin/env python3
"""Print triangles, materials, envelope (game axes) and signed volume of every GLB in
the manifest. Plain python3, no Blender:  python3 Tools/ModelGen/_stats.py [Name ...]"""
import json
import os
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import check  # noqa: E402

with open(check.MANIFEST, encoding="utf-8") as f:
    entries = json.load(f)
for e in entries:
    if sys.argv[1:] and e["name"] not in sys.argv[1:]:
        continue
    path = os.path.join(check.MODELS_DIR, e["name"] + ".glb")
    if not os.path.isfile(path):
        print("%-18s missing" % e["name"])
        continue
    g, b = check.read_glb(path)
    prims = g["meshes"][0]["primitives"]
    acc = g["accessors"]
    tris = sum(acc[p["indices"]]["count"] // 3 for p in prims)
    lo = [min(acc[p["attributes"]["POSITION"]]["min"][i] for p in prims) for i in range(3)]
    hi = [max(acc[p["attributes"]["POSITION"]]["max"][i] for p in prims) for i in range(3)]
    print("%-18s %4d/%-4d tris  min %s  max %s  vol %.5f  %s" % (
        e["name"], tris, e["max_tris"],
        [round(x, 3) for x in check.gltf_to_game(lo)], [round(x, 3) for x in check.gltf_to_game(hi)],
        check.signed_volume(g, b), ",".join(m["name"][4:] for m in g["materials"])))
