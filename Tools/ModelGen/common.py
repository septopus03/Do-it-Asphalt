"""Shared helpers for the Do it Asphalt model scripts.

Every model script is run headless:   blender -b -P Tools/ModelGen/<model>.py
and starts with:

    import os, sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import common as c

    c.reset_scene()
    parts = [c.box(...), c.cylinder(...), ...]
    obj = c.join(parts, "SM_Name")
    c.export(obj, "SM_Name")
    c.render_previews(obj, "SM_Name")

COORDINATES. Scripts write geometry directly in GAME axes: +X forward, +Y right,
+Z up, metres - the numbers from docs/03-3d-modely.md. That system is left-handed
(Unreal); Blender and glTF are right-handed. Only export() and render_previews()
deal with that: they work on a copy mirrored in Y with the winding flipped, so
the GLB holds  glTF(x, y, z) = game(x, z, y)  and the previews show the model as
the game will (+Y really is on the right when looking along +X).
Consequences for scripts:
  - never rely on Blender's viewport handedness, think in the game axes only;
  - rotate() uses the plain formula (+90 deg about Z takes +X to +Y);
  - mesh() faces are counter-clockwise seen from outside *as if the script axes
    were right-handed* (the same convention every helper here uses).

The palette part of this file (PALETTE, MAX_MATERIALS, hex_to_linear) is also
imported by check.py, which runs without Blender - keep it free of bpy.
"""
import math
import os

try:
    import bpy
    import bmesh
    from mathutils import Matrix, Vector
except ImportError:  # plain python3 (check.py): palette only
    bpy = None

# --------------------------------------------------------------------------- palette

# name -> (hex sRGB colour, metallic, roughness); must match docs/03-3d-modely.md §3.
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
MAX_MATERIALS = 5  # per model


def hex_to_linear(hex_colour):
    """'#RRGGBB' (sRGB) -> (r, g, b) linear floats, as stored in Blender and glTF."""
    h = hex_colour.lstrip("#")
    out = []
    for i in (0, 2, 4):
        c = int(h[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return tuple(out)


# --------------------------------------------------------------------------- paths

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MODELS_DIR = os.path.join(ROOT, "SourceAssets", "Models")
PREVIEWS_DIR = os.path.join(ROOT, "SourceAssets", "Previews")

PREVIEW_SIZE = (800, 600)
SILHOUETTE_HEIGHT = 1.8

# --------------------------------------------------------------------------- scene


def reset_scene():
    """Empty factory scene in metres. Call first in every model script."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0


def material(name):
    """Palette material `name`, created once per scene. Non-palette names raise."""
    if name not in PALETTE:
        raise KeyError("'%s' is not a palette material (docs/03-3d-modely.md §3)" % name)
    mat = bpy.data.materials.get(name)
    if mat is None:
        hex_colour, metallic, roughness = PALETTE[name]
        mat = _flat_material(name, hex_to_linear(hex_colour), metallic, roughness)
    return mat


def _flat_material(name, rgb, metallic, roughness):
    mat = bpy.data.materials.new(name)
    try:
        mat.use_nodes = True  # no-op / deprecated on Blender 5.x, needed before
    except Exception:
        pass
    nodes = mat.node_tree.nodes
    bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is None:
        nodes.clear()
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        out = nodes.new("ShaderNodeOutputMaterial")
        mat.node_tree.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bsdf.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    mat.diffuse_color = (rgb[0], rgb[1], rgb[2], 1.0)  # Workbench / viewport
    mat.metallic = metallic
    mat.roughness = roughness
    mat.use_backface_culling = True  # single-sided: flipped faces show up as holes
    return mat


# --------------------------------------------------------------------------- shapes

_AXIS_NAMES = ("X", "Y", "Z", "-X", "-Y", "-Z")


def _axis_matrix(axis):
    """Rotation taking the primitive's local +Z (bottom -> top) to `axis`."""
    a = str(axis).upper().replace("+", "")
    if a not in _AXIS_NAMES:
        raise ValueError("axis must be one of %s, got %r" % (", ".join(_AXIS_NAMES), axis))
    q = math.pi / 2
    return {
        "Z": Matrix.Identity(4),
        "-Z": Matrix.Rotation(math.pi, 4, "X"),
        "X": Matrix.Rotation(q, 4, "Y"),
        "-X": Matrix.Rotation(-q, 4, "Y"),
        "Y": Matrix.Rotation(-q, 4, "X"),
        "-Y": Matrix.Rotation(q, 4, "X"),
    }[a]


def _object_from_bmesh(bm, name, mat):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(material(mat) if isinstance(mat, str) else mat)
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def _box_into(bm, size, center):
    m = Matrix.Translation(Vector(center)) @ Matrix.Diagonal((size[0], size[1], size[2], 1.0))
    return bmesh.ops.create_cube(bm, size=1.0, matrix=m)["verts"]


def _frustum_bmesh(r_bottom, r_top, height, segments, cap_bottom, cap_top):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments,
                          radius1=r_bottom, radius2=r_top, depth=height)
    half = height / 2.0
    drop = []
    for f in bm.faces:
        zs = [v.co.z for v in f.verts]
        if max(zs) - min(zs) > 1e-9:
            continue  # side face
        if (zs[0] < 0 and not cap_bottom) or (zs[0] > 0 and not cap_top):
            drop.append(f)
    if drop:
        bmesh.ops.delete(bm, geom=drop, context="FACES_ONLY")
    return bm


def box(size_xyz, center_xyz, mat, bevel=0.0):
    """Axis-aligned box. `bevel` > 0 chamfers all 12 edges by that many metres
    (44 triangles instead of 12; the envelope stays size_xyz)."""
    bm = bmesh.new()
    _box_into(bm, size_xyz, center_xyz)
    if bevel > 0.0:
        bmesh.ops.bevel(bm, geom=bm.edges[:], offset=bevel, offset_type="OFFSET",
                        segments=1, profile=0.5, affect="EDGES")
    return _object_from_bmesh(bm, "box", mat)


def cone(r_bottom, r_top, height, center_xyz, axis, mat, segments=12,
         cap_bottom=True, cap_top=True):
    """Frustum (or cone when one radius is 0) around `center_xyz`.

    `axis` is the bottom -> top direction: 'X', 'Y', 'Z', '-X', '-Y' or '-Z'.
    Switch a cap off where it is hidden inside/against another part; a cap costs
    segments - 2 triangles, the side 2 * segments (segments when a radius is 0).
    The cross-section is a regular polygon with a vertex on the perpendicular
    axes, so its envelope is exactly 2 * radius when `segments` is a multiple of 4.
    """
    bm = _frustum_bmesh(r_bottom, r_top, height, segments, cap_bottom, cap_top)
    bm.transform(Matrix.Translation(Vector(center_xyz)) @ _axis_matrix(axis))
    return _object_from_bmesh(bm, "cone", mat)


def cylinder(radius, height, center_xyz, axis, mat, segments=12,
             cap_bottom=True, cap_top=True):
    """Cylinder around `center_xyz` along `axis`; see cone() for the details."""
    return cone(radius, radius, height, center_xyz, axis, mat, segments, cap_bottom, cap_top)


def beam(p0, p1, radius, mat, segments=8, radius_end=None, cap_start=True, cap_end=True):
    """Round bar from point p0 to point p1 (any direction): handles, hoses, struts.
    `radius_end` tapers it. Note the envelope of a slanted bar exceeds its end points."""
    p0, p1 = Vector(p0), Vector(p1)
    d = p1 - p0
    if d.length < 1e-9:
        raise ValueError("beam: p0 and p1 coincide")
    r1 = radius if radius_end is None else radius_end
    bm = _frustum_bmesh(radius, r1, d.length, segments, cap_start, cap_end)
    z = Vector((0.0, 0.0, 1.0))
    if d.normalized().dot(z) < -0.999999:
        rot = Matrix.Rotation(math.pi, 4, "X")
    else:
        rot = z.rotation_difference(d).to_matrix().to_4x4()
    bm.transform(Matrix.Translation((p0 + p1) / 2.0) @ rot)
    return _object_from_bmesh(bm, "beam", mat)


def bar(p0, p1, width, height, mat, cap_start=True, cap_end=True):
    """Rectangular bar from point p0 to point p1 (any direction): conveyors, ducts,
    slanted legs, braces, drawbars. `width` is measured horizontally across the bar,
    `height` in the vertical plane that contains it (so a sloping conveyor keeps
    its belt side up). For a vertical bar `width` runs along Y and `height` along X.
    12 triangles (2 less per switched-off end cap)."""
    p0, p1 = Vector(p0), Vector(p1)
    d = p1 - p0
    if d.length < 1e-9:
        raise ValueError("bar: p0 and p1 coincide")
    length = d.length
    d = d.normalized()
    up = Vector((0.0, 0.0, 1.0))
    if abs(d.dot(up)) > 0.999999:
        up = Vector((1.0, 0.0, 0.0))
    side = up.cross(d).normalized()
    top = d.cross(side)
    bm = bmesh.new()
    _box_into(bm, (width, height, length), (0.0, 0.0, 0.0))
    drop = [f for f in bm.faces
            if max(v.co.z for v in f.verts) - min(v.co.z for v in f.verts) < 1e-9
            and ((f.verts[0].co.z < 0 and not cap_start) or (f.verts[0].co.z > 0 and not cap_end))]
    if drop:
        bmesh.ops.delete(bm, geom=drop, context="FACES_ONLY")
    rot = Matrix((side, top, d)).transposed().to_4x4()
    bm.transform(Matrix.Translation((p0 + p1) / 2.0) @ rot)
    return _object_from_bmesh(bm, "bar", mat)


def ring(r_inner, r_outer, h0, h1, center_xyz, axis, mat, segments=12):
    """Closed band of rectangular section around `axis` through `center_xyz`, from
    h0 to h1 along the axis (relative to the centre): tyre rings on a drum, hoops,
    flanges, railings. Visible from inside and outside. 8 * segments triangles."""
    return lathe([(r_inner, h0), (r_outer, h0), (r_outer, h1), (r_inner, h1), (r_inner, h0)],
                 center_xyz, axis, mat, segments, cap_start=False, cap_end=False)


def mesh(verts, faces, mat):
    """Arbitrary polygons: verts = [(x, y, z), ...], faces = [(i, j, k, ...), ...],
    counter-clockwise seen from outside (see the module docstring)."""
    bm = bmesh.new()
    bv = [bm.verts.new(v) for v in verts]
    for f in faces:
        bm.faces.new([bv[i] for i in f])
    return _object_from_bmesh(bm, "mesh", mat)


def lathe(profile, center_xyz, axis, mat, segments=12, cap_start=True, cap_end=True):
    """Solid of revolution around `axis` through `center_xyz`.

    `profile` = [(radius, h), ...]: h is the position along the axis relative to
    the centre, listed from the start (bottom) end to the end (top) end with the
    OUTSIDE ON THE RIGHT of the direction of travel (radius to the right, h up) -
    i.e. a plain bottom-to-top outline; a bulge may go out and come back in.
    A point with radius 0 becomes a single apex vertex (pointed end / dome top).
    `cap_start` / `cap_end` close an end ring whose radius is not 0.
    Triangles: 2 * segments per band (segments next to an apex), segments - 2 per cap.
    Ring vertices sit on the perpendicular axes when `segments` is a multiple of 4,
    so the envelope is exactly 2 * radius. Use scale() afterwards for oval sections.
    """
    if len(profile) < 2:
        raise ValueError("lathe: profile needs at least 2 points")
    bm = bmesh.new()
    rings = []
    for r, h in profile:
        if r < 0:
            raise ValueError("lathe: negative radius")
        if r < 1e-9:
            rings.append([bm.verts.new((0.0, 0.0, h))])
        else:
            rings.append([bm.verts.new((r * math.cos(2 * math.pi * k / segments),
                                        r * math.sin(2 * math.pi * k / segments), h))
                          for k in range(segments)])
    for lo, hi in zip(rings, rings[1:]):
        for k in range(segments):
            k1 = (k + 1) % segments
            if len(lo) == 1 and len(hi) == 1:
                continue
            if len(lo) == 1:
                bm.faces.new((lo[0], hi[k1], hi[k]))
            elif len(hi) == 1:
                bm.faces.new((lo[k], lo[k1], hi[0]))
            else:
                bm.faces.new((lo[k], lo[k1], hi[k1], hi[k]))
    if cap_start and len(rings[0]) > 1:
        bm.faces.new(list(reversed(rings[0])))
    if cap_end and len(rings[-1]) > 1:
        bm.faces.new(rings[-1])
    bm.transform(Matrix.Translation(Vector(center_xyz)) @ _axis_matrix(axis))
    return _object_from_bmesh(bm, "lathe", mat)


def dome(radius, height, center_xyz, axis, mat, segments=12, rings=3, cap=True):
    """Half ellipsoid: base circle of `radius` at `center_xyz`, apex `height` along
    `axis`. `rings` bands of faces (the last one is a triangle fan); `cap` closes
    the flat base. Triangles: segments * (2 * rings - 1) (+ segments - 2)."""
    profile = [(radius * math.cos(math.pi / 2 * i / rings), height * math.sin(math.pi / 2 * i / rings))
               for i in range(rings)] + [(0.0, height)]
    return lathe(profile, center_xyz, axis, mat, segments, cap_start=cap)


def extrude(outline, lo, hi, axis, mat, cap_lo=True, cap_hi=True):
    """Prism: a flat polygon swept along a principal axis ('X', 'Y' or 'Z') from
    coordinate `lo` to `hi` - wedges, hoods, visors, brackets, plates.

    `outline` = [(u, v), ...] in the plane across the axis, in the cyclic order of
    the axes:  axis 'X' -> (u, v) = (y, z),  'Y' -> (z, x),  'Z' -> (x, y),
    listed counter-clockwise in that (u, v) plane (clockwise input is detected
    and reversed). It may be concave but must not cross itself.
    Triangles: 2 * n for the sides, n - 2 per cap.
    """
    a = str(axis).upper()
    if a not in ("X", "Y", "Z"):
        raise ValueError("extrude: axis must be 'X', 'Y' or 'Z'")
    if hi < lo:
        lo, hi = hi, lo
    pts = [tuple(p) for p in outline]
    area = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1]
               for i in range(len(pts)))
    if area < 0:
        pts.reverse()
    place = {"X": lambda u, v, w: (w, u, v),
             "Y": lambda u, v, w: (v, w, u),
             "Z": lambda u, v, w: (u, v, w)}[a]
    bm = bmesh.new()
    bottom = [bm.verts.new(place(u, v, lo)) for u, v in pts]
    top = [bm.verts.new(place(u, v, hi)) for u, v in pts]
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    if cap_lo:
        bm.faces.new(list(reversed(bottom)))
    if cap_hi:
        bm.faces.new(top)
    return _object_from_bmesh(bm, "extrude", mat)


def scale(obj, factors_xyz, pivot=(0.0, 0.0, 0.0)):
    """Scale a part about `pivot` (round -> oval tanks, domes -> helmet shells).
    Factors must be positive; use mirror() to flip. Baked into the vertices. Returns obj."""
    if min(factors_xyz) <= 0:
        raise ValueError("scale: factors must be positive (use mirror() to flip)")
    p = Vector(pivot)
    obj.data.transform(Matrix.Translation(p) @ Matrix.Diagonal((*factors_xyz, 1.0))
                       @ Matrix.Translation(-p))
    return obj


def copy(obj):
    """Independent duplicate of a part (same material)."""
    twin = bpy.data.objects.new(obj.name, obj.data.copy())
    twin.matrix_world = obj.matrix_world.copy()
    bpy.context.scene.collection.objects.link(twin)
    return twin


def mirror(obj, axis, at=0.0):
    """NEW part: `obj` reflected in the plane  axis = at  ('X', 'Y' or 'Z'), with the
    winding flipped back so the faces still point outward. `obj` itself is kept:
        parts += [strap, c.mirror(strap, "Y")]"""
    i = "XYZ".index(str(axis).upper())
    twin = copy(obj)
    factors = [1.0, 1.0, 1.0, 1.0]
    factors[i] = -1.0
    offset = [0.0, 0.0, 0.0]
    offset[i] = 2.0 * at
    twin.data.transform(Matrix.Translation(Vector(offset)) @ Matrix.Diagonal(factors))
    twin.data.flip_normals()
    return twin


def assert_outward(parts):
    """Raise if any part (nested lists allowed) is wound inside-out. Measured about
    the part's own centre, so it also works for parts with their caps switched off.
    Call it on the part list just before join()."""
    for part in _flatten(parts):
        mn, mx = bounds(part)
        centre = Vector([(mn[i] + mx[i]) / 2.0 for i in range(3)])
        bm = bmesh.new()
        bm.from_mesh(part.data)
        bmesh.ops.translate(bm, verts=bm.verts[:], vec=-centre)
        v = bm.calc_volume(signed=True)
        bm.free()
        if v <= 0.0:
            raise ValueError("part '%s' at %s is wound inside-out (signed volume %g)"
                             % (part.name, tuple(round(x, 3) for x in centre), v))
    return parts


def signed_volume(obj):
    """Signed volume in m^3. Positive when the faces point outward (what Unreal's
    import requires); for a closed part it is its real volume."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    v = bm.calc_volume(signed=True)
    bm.free()
    return v


def rotate(obj, axis, degrees, pivot=(0.0, 0.0, 0.0)):
    """Rotate a part about a line through `pivot` parallel to 'X', 'Y' or 'Z'.
    +90 deg about Z takes +X to +Y, about X takes +Y to +Z, about Y takes +Z to +X.
    Baked into the vertices. Returns obj."""
    p = Vector(pivot)
    m = Matrix.Translation(p) @ Matrix.Rotation(math.radians(degrees), 4, axis.upper()) \
        @ Matrix.Translation(-p)
    obj.data.transform(m)
    return obj


def translate(obj, offset_xyz):
    """Move a part; baked into the vertices. Returns obj."""
    obj.data.transform(Matrix.Translation(Vector(offset_xyz)))
    return obj


def bounds(obj):
    """((min x, y, z), (max x, y, z)) of an object, in game axes."""
    pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return (tuple(min(p[i] for p in pts) for i in range(3)),
            tuple(max(p[i] for p in pts) for i in range(3)))


def _flatten(items):
    for it in items:
        if isinstance(it, (list, tuple)):
            yield from _flatten(it)
        elif it is not None:
            yield it


def join(objects, name):
    """Merge parts (nested lists allowed) into ONE mesh object called `name`:
    transforms applied, flat shading, one slot per palette material in order of
    first use. The parts are removed from the scene."""
    parts = list(_flatten(objects))
    if not parts:
        raise ValueError("join: no objects")
    bm = bmesh.new()
    slots = []
    for part in parts:
        me = part.data
        remap = []
        for m in me.materials:
            if m not in slots:
                slots.append(m)
            remap.append(slots.index(m))
        first_vert, first_face = len(bm.verts), len(bm.faces)
        bm.from_mesh(me)
        bm.verts.ensure_lookup_table()
        bm.faces.ensure_lookup_table()
        world = part.matrix_world.copy()
        for v in bm.verts[first_vert:]:
            v.co = world @ v.co
        for f in bm.faces[first_face:]:
            f.material_index = remap[f.material_index] if remap else 0
            f.smooth = False
    if len(slots) > MAX_MATERIALS:
        print("WARNING %s uses %d materials, limit is %d" % (name, len(slots), MAX_MATERIALS))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in slots:
        me.materials.append(m)
    for part in parts:
        old = part.data
        bpy.data.objects.remove(part)
        if old.users == 0:
            bpy.data.meshes.remove(old)
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


# --------------------------------------------------------------------------- export


def _game_to_blender(v):
    return Vector((v[0], -v[1], v[2]))


def _mirrored_copy(obj, name):
    """Right-handed twin of a game-axes object: Y negated, winding flipped."""
    me = obj.data.copy()
    me.transform(obj.matrix_world @ Matrix.Identity(4))
    me.transform(Matrix.Diagonal((1.0, -1.0, 1.0, 1.0)))
    me.flip_normals()
    me.name = name
    twin = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(twin)
    return twin


def _remove(obj):
    data = obj.data
    bpy.data.objects.remove(obj)
    if data is not None and data.users == 0:
        if isinstance(data, bpy.types.Mesh):
            bpy.data.meshes.remove(data)
        elif isinstance(data, bpy.types.Camera):
            bpy.data.cameras.remove(data)
        elif isinstance(data, bpy.types.Light):
            bpy.data.lights.remove(data)


def export(obj, name):
    """Write SourceAssets/Models/<name>.glb and return its path.

    `obj` is in game axes (+X forward, +Y right, +Z up, metres). The file holds
    glTF(x, y, z) = game(x, z, y): glTF's +Y-up, right-handed, metres. check.py
    and Unreal's glTF import apply the inverse swap.
    """
    os.makedirs(MODELS_DIR, exist_ok=True)
    path = os.path.join(MODELS_DIR, name + ".glb")
    saved = (obj.name, obj.data.name)
    obj.name = obj.data.name = name + ".game"  # free the name for the twin
    twin = _mirrored_copy(obj, name)
    twin.name = twin.data.name = name
    for o in bpy.context.scene.objects:
        o.select_set(False)
    twin.select_set(True)
    bpy.context.view_layer.objects.active = twin
    options = dict(
        filepath=path, export_format="GLB", use_selection=True, export_apply=True,
        export_yup=True, export_materials="EXPORT", export_normals=True,
        export_texcoords=False, export_tangents=False, export_vertex_color="NONE",
        export_animations=False, export_skins=False, export_morph=False,
        export_cameras=False, export_lights=False, export_extras=False,
    )
    known = {p.identifier for p in bpy.ops.export_scene.gltf.get_rna_type().properties}
    try:
        result = bpy.ops.export_scene.gltf(**{k: v for k, v in options.items() if k in known})
    finally:
        _remove(twin)
        obj.name, obj.data.name = saved
    if "FINISHED" not in result:
        raise RuntimeError("glTF export of %s failed: %s" % (name, result))
    print("EXPORTED %s" % path)
    return path


# --------------------------------------------------------------------------- previews


def _silhouette(position_blender):
    """Blocky 1.8 m person, feet at `position_blender`, facing +X."""
    bm = bmesh.new()
    for size, center in (
        ((0.15, 0.13, 0.50), (0.0, 0.09, 0.25)),     # shins
        ((0.15, 0.13, 0.50), (0.0, -0.09, 0.25)),
        ((0.16, 0.14, 0.33), (0.0, 0.09, 0.685)),    # thighs (gap at 0.50-0.52 = knee)
        ((0.16, 0.14, 0.33), (0.0, -0.09, 0.685)),
        ((0.20, 0.36, 0.60), (0.0, 0.0, 1.15)),      # torso
        ((0.12, 0.09, 0.62), (0.0, 0.235, 1.11)),    # arms
        ((0.12, 0.09, 0.62), (0.0, -0.235, 1.11)),
        ((0.10, 0.10, 0.10), (0.0, 0.0, 1.50)),      # neck
        ((0.20, 0.18, 0.25), (0.02, 0.0, 1.675)),    # head, top at 1.80
    ):
        _box_into(bm, size, center)
    bm.transform(Matrix.Translation(position_blender))
    grey = _flat_material("_PreviewSilhouette", (0.25, 0.25, 0.27), 0.0, 0.9)
    return _object_from_bmesh(bm, "_PreviewSilhouette", grey)


def _look_matrix(location, back, up_hint):
    """Camera matrix at `location` looking along -back."""
    b = Vector(back).normalized()
    r = Vector(up_hint).cross(b).normalized()
    u = b.cross(r)
    m = Matrix((r, u, b)).transposed().to_4x4()
    m.translation = location
    return m


def _set_engine(scene):
    for engine in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT", "BLENDER_WORKBENCH"):
        try:
            scene.render.engine = engine
        except TypeError:
            continue
        break
    if scene.render.engine == "BLENDER_WORKBENCH":
        _setup_workbench(scene)
    else:
        try:
            scene.eevee.taa_render_samples = 32
        except Exception:
            pass
    return scene.render.engine


def _setup_workbench(scene):
    scene.render.engine = "BLENDER_WORKBENCH"
    shading = scene.display.shading
    shading.light = "STUDIO"
    shading.color_type = "MATERIAL"
    shading.show_backface_culling = True
    shading.show_shadows = True
    shading.show_cavity = True


def render_previews(obj, name):
    """Render SourceAssets/Previews/<name>_a.png (three-quarter front, perspective),
    _b.png (right side, orthographic) and _c.png (top, orthographic, +X up),
    800x600, with a dark grey 1.8 m figure standing behind-right of the model
    (models smaller than 0.4 m are framed with only its lower 1.0 m).
    The scene is left as it was; nothing from here reaches the GLB."""
    os.makedirs(PREVIEWS_DIR, exist_ok=True)
    scene = bpy.context.scene
    mn, mx = bounds(obj)
    largest = max(mx[i] - mn[i] for i in range(3))
    floor = min(0.0, mn[2])
    gap = min(max(0.5 * largest, 0.10), 0.30)
    sil_game = (mn[0] - gap - 0.10, mx[1] + gap + 0.28, floor)
    sil_top = floor + (SILHOUETTE_HEIGHT if largest >= 0.4 else 1.0)

    temp = []
    twin = _mirrored_copy(obj, name + ".preview")
    temp.append(twin)
    temp.append(_silhouette(_game_to_blender(sil_game)))

    # What the cameras must frame, as flat Blender coordinates.
    lo = (min(mn[0], sil_game[0] - 0.10), min(mn[1], sil_game[1] - 0.28), floor)
    hi = (max(mx[0], sil_game[0] + 0.12), max(mx[1], sil_game[1] + 0.28), max(mx[2], sil_top))
    corners = [_game_to_blender((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1])
               for z in (lo[2], hi[2])]
    flat = [c for p in corners for c in p]
    centre = sum(corners, Vector()) / 8.0
    radius = max((p - centre).length for p in corners)

    # Ground disc under everything.
    bm = bmesh.new()
    bmesh.ops.create_circle(bm, cap_ends=True, segments=48, radius=radius * 6.0)
    bm.transform(Matrix.Translation((centre.x, centre.y, floor - 0.002)))
    ground = _object_from_bmesh(
        bm, "_PreviewGround", _flat_material("_PreviewGround", (0.42, 0.43, 0.45), 0.0, 1.0))
    temp.append(ground)

    # Lights: key sun with shadows, shadowless fill from the other side, bright world.
    def sun(label, from_game, energy, shadow):
        data = bpy.data.lights.new(label, "SUN")
        data.energy = energy
        data.use_shadow = shadow
        light = bpy.data.objects.new(label, data)
        light.matrix_world = _look_matrix(Vector(), _game_to_blender(from_game), (0, 0, 1))
        scene.collection.objects.link(light)
        temp.append(light)

    sun("_PreviewKey", (0.55, 0.65, 1.0), 2.0, True)
    sun("_PreviewFill", (-0.6, -0.5, 0.35), 0.6, False)
    sun("_PreviewFront", (1.0, -0.3, 0.3), 0.4, False)

    world = bpy.data.worlds.new("_PreviewWorld")
    try:
        world.use_nodes = True
    except Exception:
        pass
    world.color = (0.80, 0.82, 0.86)
    if world.node_tree is not None:
        bg = next((n for n in world.node_tree.nodes if n.type == "BACKGROUND"), None)
        if bg is not None:
            bg.inputs["Color"].default_value = (0.80, 0.82, 0.86, 1.0)
            bg.inputs["Strength"].default_value = 0.6
    old_world, scene.world = scene.world, world

    render = scene.render
    render.resolution_x, render.resolution_y = PREVIEW_SIZE
    render.resolution_percentage = 100
    render.film_transparent = False
    render.image_settings.file_format = "PNG"
    render.image_settings.color_mode = "RGB"
    render.image_settings.compression = 100
    render.dither_intensity = 0.0  # with the stamps off: byte-identical PNGs on re-runs
    for prop in render.bl_rna.properties:
        if prop.identifier.startswith("use_stamp"):
            setattr(render, prop.identifier, False)
    scene.view_settings.view_transform = "Standard"
    try:
        scene.view_settings.look = "None"
    except TypeError:
        pass
    engine = _set_engine(scene)

    cam_data = bpy.data.cameras.new("_PreviewCamera")
    cam_data.clip_start = 0.01
    cam_data.clip_end = 1000.0
    cam = bpy.data.objects.new("_PreviewCamera", cam_data)
    scene.collection.objects.link(cam)
    temp.append(cam)
    old_camera, scene.camera = scene.camera, cam
    # Only the mirrored twin may be seen, not the game-axes originals.
    hidden = [o for o in scene.objects if o not in temp and not o.hide_render]
    for o in hidden:
        o.hide_render = True

    views = (  # suffix, camera position relative to the subject (game axes), type, up hint
        ("a", (1.0, 0.75, 0.55), "PERSP", (0, 0, 1)),
        ("b", (0.0, 1.0, 0.12), "ORTHO", (0, 0, 1)),
        ("c", (0.0, 0.0, 1.0), "ORTHO", None),
    )
    paths = []
    try:
        for suffix, from_game, cam_type, up_hint in views:
            back = _game_to_blender(from_game).normalized()
            cam_data.type = cam_type
            cam_data.lens = 50.0
            if up_hint is None:  # straight down: game +X up, game +Y right on screen
                cam.matrix_world = Matrix(((0, 1, 0, 0), (-1, 0, 0, 0), (0, 0, 1, 0),
                                           (0, 0, 0, 1))) @ Matrix.Identity(4)
                cam.location = centre + back * radius * 10.0
            else:
                cam.matrix_world = _look_matrix(centre + back * radius * 10.0, back, up_hint)
            bpy.context.view_layer.update()
            if cam_type == "ORTHO":
                # Fit by hand: project the corners on the camera's right / up axes.
                right, up = cam.matrix_world.col[0].xyz, cam.matrix_world.col[1].xyz
                us = [(p - centre).dot(right) for p in corners]
                vs = [(p - centre).dot(up) for p in corners]
                aspect = PREVIEW_SIZE[0] / PREVIEW_SIZE[1]
                cam_data.ortho_scale = 1.12 * max(max(us) - min(us), (max(vs) - min(vs)) * aspect)
                cam.location = (centre + right * (max(us) + min(us)) / 2.0
                                + up * (max(vs) + min(vs)) / 2.0 + back * radius * 10.0)
            else:
                location, _ = cam.camera_fit_coords(
                    bpy.context.evaluated_depsgraph_get(), flat)
                to_centre = (centre - Vector(location)).length
                cam.location = Vector(location) + back * to_centre * 0.10
            path = os.path.join(PREVIEWS_DIR, "%s_%s.png" % (name, suffix))
            render.filepath = path
            try:
                bpy.ops.render.render(write_still=True)
            except RuntimeError as e:
                if scene.render.engine == "BLENDER_WORKBENCH":
                    raise
                print("WARNING %s failed headless (%s), using Workbench" % (engine, e))
                _setup_workbench(scene)
                engine = scene.render.engine
                bpy.ops.render.render(write_still=True)
            paths.append(path)
    finally:
        for o in hidden:
            o.hide_render = False
        scene.camera = old_camera
        scene.world = old_world
        bpy.data.worlds.remove(world)
        for o in temp:
            mats = list(o.data.materials) if isinstance(o.data, bpy.types.Mesh) else []
            _remove(o)
            for m in mats:
                if m.name.startswith("_Preview") and m.users == 0:
                    bpy.data.materials.remove(m)
    print("PREVIEWS %s (%s): %s" % (name, engine, ", ".join(os.path.basename(p) for p in paths)))
    return paths
