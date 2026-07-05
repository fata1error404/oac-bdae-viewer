#!/usr/bin/env python3
"""Convert a small static COLLADA subset to OaC 32-bit BDAE.

This is an intentionally narrow first pass. It writes an outer .bdae ZIP
containing a 32-bit little_endian_not_quantized.bdae payload compatible with
the old 0.0.0.779 layout documented by aux_docs/BDAE_model_1.3.0.bt.
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple
from xml.etree import ElementTree as ET


BDAE_VERSION_STRING = "0,0,0,779"
INNER_BDAE_NAME = "little_endian_not_quantized.bdae"


Vec2 = Tuple[float, float]
Vec3 = Tuple[float, float, float]
Quat = Tuple[float, float, float, float]


@dataclass
class Vertex:
    position: Vec3
    normal: Vec3
    uv: Vec2


@dataclass
class Submesh:
    material: str
    indices: List[int]


@dataclass
class Mesh:
    name: str
    vertices: List[Vertex] = field(default_factory=list)
    submeshes: List[Submesh] = field(default_factory=list)
    translation: Vec3 = (0.0, 0.0, 0.0)
    rotation: Quat = (0.0, 0.0, 0.0, 1.0)
    scale: Vec3 = (1.0, 1.0, 1.0)


@dataclass
class Skin:
    name: str
    mesh_name: str
    bind_shape_matrix: List[float]
    bone_names: List[str]
    inverse_bind_matrices: List[List[float]]
    influences: List[List[Tuple[int, float]]]
    max_influence: int
    bounding_boxes: List[Tuple[Vec3, Vec3]] = field(default_factory=list)


@dataclass
class Node:
    node_id: str
    name: str
    sid: str
    translation: Vec3 = (0.0, 0.0, 0.0)
    rotation: Quat = (0.0, 0.0, 0.0, 1.0)
    scale: Vec3 = (1.0, 1.0, 1.0)
    children: List["Node"] = field(default_factory=list)


@dataclass
class Scene:
    meshes: List[Mesh]
    material_to_texture: Dict[str, str]
    skins: List[Skin] = field(default_factory=list)
    root_nodes: List[Node] = field(default_factory=list)


class StringTable:
    def __init__(self) -> None:
        self._offsets: Dict[str, int] = {}
        self._items: List[str] = []

    def add(self, value: str) -> int:
        if value not in self._offsets:
            self._offsets[value] = -1
            self._items.append(value)
        return self._offsets[value]

    def build(self, start_offset: int) -> Tuple[bytes, Dict[str, int]]:
        out = bytearray()
        offsets: Dict[str, int] = {}

        for value in self._items:
            encoded = value.encode("utf-8")
            out += u32(len(encoded))
            offsets[value] = start_offset + len(out)
            out += encoded

            # The engine string table stores enough padding for a trailing NUL
            # and 4-byte alignment, while string lengths exclude the NUL.
            padded_len = 0 if not encoded else ((len(encoded) // 4) + 1) * 4
            out += b"\0" * (padded_len - len(encoded))

        self._offsets = offsets
        return bytes(out), offsets


class BinaryBuilder:
    def __init__(self) -> None:
        self.data = bytearray()

    def tell(self) -> int:
        return len(self.data)

    def align(self, size: int = 4) -> None:
        while len(self.data) % size:
            self.data.append(0)

    def write(self, blob: bytes) -> int:
        pos = len(self.data)
        self.data += blob
        return pos

    def patch(self, offset: int, blob: bytes) -> None:
        self.data[offset : offset + len(blob)] = blob


def u16(value: int) -> bytes:
    return struct.pack("<H", value)


def s32(value: int) -> bytes:
    return struct.pack("<i", value)


def u32(value: int) -> bytes:
    return struct.pack("<I", value)


def f32(value: float) -> bytes:
    return struct.pack("<f", value)


def vec2(v: Vec2) -> bytes:
    return f32(v[0]) + f32(v[1])


def vec3(v: Vec3) -> bytes:
    return f32(v[0]) + f32(v[1]) + f32(v[2])


def quat(q: Quat) -> bytes:
    return f32(q[0]) + f32(q[1]) + f32(q[2]) + f32(q[3])


def clean_id(value: str) -> str:
    value = value.strip()
    return value[1:] if value.startswith("#") else value


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def children(elem: ET.Element, name: str) -> Iterable[ET.Element]:
    for child in elem:
        if local_name(child.tag) == name:
            yield child


def child(elem: ET.Element, name: str) -> Optional[ET.Element]:
    return next(children(elem, name), None)


def all_desc(elem: ET.Element, name: str) -> Iterable[ET.Element]:
    for item in elem.iter():
        if local_name(item.tag) == name:
            yield item


def parse_floats(text: Optional[str]) -> List[float]:
    if not text:
        return []
    return [float(x) for x in text.split()]


def parse_ints(text: Optional[str]) -> List[int]:
    if not text:
        return []
    return [int(x) for x in text.split()]


def normalize(v: Vec3) -> Vec3:
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if length == 0.0:
        return (0.0, 1.0, 0.0)
    return (v[0] / length, v[1] / length, v[2] / length)


def quat_from_axis_angle(axis: Vec3, angle_degrees: float) -> Quat:
    axis = normalize(axis)
    half = math.radians(angle_degrees) * 0.5
    s = math.sin(half)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half))


def quat_mul(a: Quat, b: Quat) -> Quat:
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    x = aw * bx + ax * bw + ay * bz - az * by
    y = aw * by - ax * bz + ay * bw + az * bx
    z = aw * bz + ax * by - ay * bx + az * bw
    w = aw * bw - ax * bx - ay * by - az * bz
    return (x, y, z, w)


def dae_quat_to_bdae(q: Quat) -> Quat:
    return (-q[0], -q[1], -q[2], q[3])


def source_values(source: ET.Element) -> Tuple[List[float], int]:
    float_array = child(source, "float_array")
    if float_array is None:
        return [], 1

    stride = 1
    technique = child(source, "technique_common")
    if technique is not None:
        accessor = child(technique, "accessor")
        if accessor is not None:
            stride = int(accessor.attrib.get("stride", "1"))

    return parse_floats(float_array.text), stride


def source_names(source: ET.Element) -> List[str]:
    name_array = child(source, "Name_array")
    if name_array is None or not name_array.text:
        return []
    return name_array.text.split()


def transpose_matrix4(values: Sequence[float]) -> List[float]:
    if len(values) != 16:
        return [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]
    return [values[row + col * 4] for row in range(4) for col in range(4)]


def read_source_tuple(
    sources: Dict[str, Tuple[List[float], int]],
    source_id: Optional[str],
    index: int,
    fallback: Sequence[float],
) -> Tuple[float, ...]:
    if not source_id or source_id not in sources:
        return tuple(fallback)

    values, stride = sources[source_id]
    start = index * stride
    if start + stride > len(values):
        return tuple(fallback)
    return tuple(values[start : start + stride])


def parse_material_textures(root: ET.Element) -> Dict[str, str]:
    images: Dict[str, str] = {}
    for image in all_desc(root, "image"):
        image_id = image.attrib.get("id")
        init_from = child(image, "init_from")
        if image_id and init_from is not None and init_from.text:
            images[image_id] = Path(init_from.text.strip()).name

    effect_textures: Dict[str, str] = {}
    for effect in all_desc(root, "effect"):
        effect_id = effect.attrib.get("id")
        if not effect_id:
            continue

        texture_name = ""
        for init_from in all_desc(effect, "init_from"):
            if init_from.text:
                ref = clean_id(init_from.text.strip())
                texture_name = images.get(ref, Path(ref).name)
                break
        if texture_name:
            effect_textures[effect_id] = texture_name

    material_to_texture: Dict[str, str] = {}
    for material in all_desc(root, "material"):
        mat_id = material.attrib.get("id") or material.attrib.get("name")
        if not mat_id:
            continue

        texture_name = ""
        instance_effect = child(material, "instance_effect")
        if instance_effect is not None:
            for init_from in all_desc(instance_effect, "init_from"):
                if init_from.text:
                    ref = clean_id(init_from.text.strip())
                    texture_name = images.get(ref, Path(ref).name)
                    break

            if not texture_name:
                effect_id = clean_id(instance_effect.attrib.get("url", "").split("#")[-1])
                texture_name = effect_textures.get(effect_id, "")

        if not texture_name and images:
            texture_name = next(iter(images.values()))

        material_to_texture[mat_id] = texture_name or f"{mat_id}.png"
        if material.attrib.get("name"):
            material_to_texture[material.attrib["name"]] = material_to_texture[mat_id]

    return material_to_texture


def parse_skins(root: ET.Element) -> List[Skin]:
    skins: List[Skin] = []

    for controller in all_desc(root, "controller"):
        skin_elem = child(controller, "skin")
        if skin_elem is None:
            continue

        controller_id = controller.attrib.get("id", "skin")
        mesh_name = skin_elem.attrib.get("source", "")

        float_sources: Dict[str, Tuple[List[float], int]] = {}
        name_sources: Dict[str, List[str]] = {}
        for source in children(skin_elem, "source"):
            source_id = source.attrib.get("id")
            if not source_id:
                continue
            if child(source, "Name_array") is not None:
                name_sources[source_id] = source_names(source)
            else:
                float_sources[source_id] = source_values(source)

        bind_shape_elem = child(skin_elem, "bind_shape_matrix")
        bind_shape = parse_floats(bind_shape_elem.text if bind_shape_elem is not None else None)
        if len(bind_shape) != 16:
            bind_shape = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]

        joints_elem = child(skin_elem, "joints")
        joint_source = ""
        bind_pose_source = ""
        if joints_elem is not None:
            for inp in children(joints_elem, "input"):
                semantic = inp.attrib.get("semantic")
                source_id = clean_id(inp.attrib.get("source", ""))
                if semantic == "JOINT":
                    joint_source = source_id
                elif semantic == "INV_BIND_MATRIX":
                    bind_pose_source = source_id

        bone_names = name_sources.get(joint_source, [])
        bind_pose_values, bind_pose_stride = float_sources.get(bind_pose_source, ([], 16))
        inverse_bind_matrices: List[List[float]] = []
        for i in range(len(bone_names)):
            start = i * bind_pose_stride
            inverse_bind_matrices.append(transpose_matrix4(bind_pose_values[start : start + 16]))

        vertex_weights = child(skin_elem, "vertex_weights")
        influences: List[List[Tuple[int, float]]] = []
        max_influence = 0
        if vertex_weights is not None:
            joint_offset = 0
            weight_offset = 1
            tuple_stride = 2
            weight_source = ""

            for inp in children(vertex_weights, "input"):
                offset = int(inp.attrib.get("offset", "0"))
                tuple_stride = max(tuple_stride, offset + 1)
                if inp.attrib.get("semantic") == "JOINT":
                    joint_offset = offset
                elif inp.attrib.get("semantic") == "WEIGHT":
                    weight_offset = offset
                    weight_source = clean_id(inp.attrib.get("source", ""))

            weights, _ = float_sources.get(weight_source, ([], 1))
            vcount_elem = child(vertex_weights, "vcount")
            v_elem = child(vertex_weights, "v")
            vcounts = parse_ints(vcount_elem.text if vcount_elem is not None else None)
            raw = parse_ints(v_elem.text if v_elem is not None else None)
            cursor = 0
            for count in vcounts:
                vertex_influences: List[Tuple[int, float]] = []
                for _ in range(count):
                    entry = raw[cursor : cursor + tuple_stride]
                    cursor += tuple_stride
                    joint_index = entry[joint_offset]
                    weight_index = entry[weight_offset]
                    weight = weights[weight_index] if 0 <= weight_index < len(weights) else 0.0
                    if weight > 0.0:
                        vertex_influences.append((joint_index, weight))

                max_influence = max(max_influence, len(vertex_influences))
                influences.append(vertex_influences)

        max_influence = min(max(max_influence, 1), 4)

        bbox_min: List[Vec3] = []
        bbox_max: List[Vec3] = []
        for source in all_desc(skin_elem, "source"):
            source_id = source.attrib.get("id", "")
            if source_id.endswith("bounding_box_min"):
                values, stride = source_values(source)
                bbox_min = [(values[i], values[i + 1], values[i + 2]) for i in range(0, len(values), stride) if i + 2 < len(values)]
            elif source_id.endswith("bounding_box_max"):
                values, stride = source_values(source)
                bbox_max = [(values[i], values[i + 1], values[i + 2]) for i in range(0, len(values), stride) if i + 2 < len(values)]

        bounding_boxes: List[Tuple[Vec3, Vec3]] = []
        for i in range(min(len(bbox_min), len(bbox_max), len(bone_names))):
            bounding_boxes.append((bbox_min[i], bbox_max[i]))

        skins.append(
            Skin(
                name=controller_id,
                mesh_name=mesh_name,
                bind_shape_matrix=transpose_matrix4(bind_shape),
                bone_names=bone_names,
                inverse_bind_matrices=inverse_bind_matrices,
                influences=influences,
                max_influence=max_influence,
                bounding_boxes=bounding_boxes,
            )
        )

    return skins


def parse_node(elem: ET.Element) -> Node:
    translation: Vec3 = (0.0, 0.0, 0.0)
    rotation: Quat = (0.0, 0.0, 0.0, 1.0)
    scale: Vec3 = (1.0, 1.0, 1.0)
    saw_matrix = False

    for transform in elem:
        name = local_name(transform.tag)
        values = parse_floats(transform.text)
        if name == "translate" and len(values) >= 3:
            translation = (values[0], values[1], values[2])
        elif name == "scale" and len(values) >= 3:
            scale = (values[0], values[1], values[2])
        elif name == "rotate" and len(values) >= 4:
            rotation = quat_mul(rotation, quat_from_axis_angle((values[0], values[1], values[2]), values[3]))
        elif name == "matrix":
            saw_matrix = True

    if saw_matrix:
        print(
            f"warning: node '{elem.attrib.get('id', elem.attrib.get('name', ''))}' "
            "uses matrix transform; dae2bdae.py currently writes identity/TRS only",
            file=sys.stderr,
        )

    return Node(
        node_id=elem.attrib.get("id", elem.attrib.get("name", "")),
        name=elem.attrib.get("name", ""),
        sid=elem.attrib.get("sid", ""),
        translation=translation,
        rotation=dae_quat_to_bdae(rotation),
        scale=scale,
        children=[parse_node(child_elem) for child_elem in children(elem, "node")],
    )


def parse_visual_scene(root: ET.Element) -> List[Node]:
    scene = child(root, "scene")
    visual_scene_id = ""

    if scene is not None:
        instance_visual_scene = child(scene, "instance_visual_scene")
        if instance_visual_scene is not None:
            visual_scene_id = clean_id(instance_visual_scene.attrib.get("url", ""))

    visual_scene = None
    for candidate in all_desc(root, "visual_scene"):
        if not visual_scene_id or candidate.attrib.get("id") == visual_scene_id:
            visual_scene = candidate
            break

    if visual_scene is None:
        return []

    return [parse_node(node_elem) for node_elem in children(visual_scene, "node")]


def collect_instance_transforms(nodes: Sequence[Node], root: ET.Element) -> Dict[str, Tuple[Vec3, Quat, Vec3]]:
    element_by_id = {elem.attrib.get("id", ""): elem for elem in all_desc(root, "node") if elem.attrib.get("id")}
    node_by_id: Dict[str, Node] = {}

    def register(node: Node) -> None:
        if node.node_id:
            node_by_id[node.node_id] = node
        for child_node in node.children:
            register(child_node)

    for root_node in nodes:
        register(root_node)

    transforms: Dict[str, Tuple[Vec3, Quat, Vec3]] = {}
    for node_id, elem in element_by_id.items():
        parsed = node_by_id.get(node_id)
        if parsed is None:
            continue

        for inst_name in ("instance_geometry", "instance_controller"):
            for instance in children(elem, inst_name):
                url = instance.attrib.get("url")
                if url:
                    transforms[clean_id(url)] = (parsed.translation, parsed.rotation, parsed.scale)

    return transforms


def parse_mesh_geometry(geometry: ET.Element, material_to_texture: Dict[str, str]) -> Mesh:
    geom_id = geometry.attrib.get("id") or "mesh"
    geom_name = geometry.attrib.get("name") or geom_id
    mesh_elem = child(geometry, "mesh")
    if mesh_elem is None:
        raise ValueError(f"geometry '{geom_id}' has no mesh")

    sources: Dict[str, Tuple[List[float], int]] = {}
    for source in children(mesh_elem, "source"):
        source_id = source.attrib.get("id")
        if source_id:
            sources[source_id] = source_values(source)

    vertices_inputs: Dict[str, str] = {}
    for vertices in children(mesh_elem, "vertices"):
        vertices_id = vertices.attrib.get("id")
        if not vertices_id:
            continue
        for inp in children(vertices, "input"):
            semantic = inp.attrib.get("semantic")
            source = clean_id(inp.attrib.get("source", ""))
            if semantic and source:
                vertices_inputs[f"{vertices_id}:{semantic}"] = source

    mesh = Mesh(name=geom_name)
    vertex_lookup: Dict[Tuple[Optional[int], Optional[int], Optional[int]], int] = {}

    primitive_elems = [e for e in mesh_elem if local_name(e.tag) in {"triangles", "polylist"}]
    if not primitive_elems:
        raise ValueError(f"geometry '{geom_id}' has no triangles/polylist")

    for primitive in primitive_elems:
        prim_name = local_name(primitive.tag)
        material = primitive.attrib.get("material") or next(iter(material_to_texture), "default")
        inputs = []
        max_offset = 0

        for inp in children(primitive, "input"):
            semantic = inp.attrib.get("semantic", "")
            source = clean_id(inp.attrib.get("source", ""))
            offset = int(inp.attrib.get("offset", "0"))
            max_offset = max(max_offset, offset)
            inputs.append((semantic, source, offset))

        tuple_stride = max_offset + 1
        p_elem = child(primitive, "p")
        raw_indices = parse_ints(p_elem.text if p_elem is not None else None)

        if prim_name == "polylist":
            vcount_elem = child(primitive, "vcount")
            vcount = parse_ints(vcount_elem.text if vcount_elem is not None else None)
            if any(count != 3 for count in vcount):
                raise ValueError(f"geometry '{geom_id}' has non-triangle polylist; triangulate the DAE first")

        submesh_indices: List[int] = []
        for i in range(0, len(raw_indices), tuple_stride):
            index_tuple = raw_indices[i : i + tuple_stride]
            position_index: Optional[int] = None
            normal_index: Optional[int] = None
            uv_index: Optional[int] = None

            for semantic, source, offset in inputs:
                index = index_tuple[offset]
                if semantic == "VERTEX":
                    position_index = index
                    normal_index = index
                    uv_index = index
                    if f"{source}:POSITION" in vertices_inputs:
                        position_index = index
                    if f"{source}:NORMAL" not in vertices_inputs:
                        normal_index = None
                    if f"{source}:TEXCOORD" not in vertices_inputs:
                        uv_index = None
                elif semantic == "POSITION":
                    position_index = index
                elif semantic == "NORMAL":
                    normal_index = index
                elif semantic == "TEXCOORD" and uv_index is None:
                    uv_index = index

            key = (position_index, normal_index, uv_index)
            if key not in vertex_lookup:
                vertex_lookup[key] = len(mesh.vertices)

                position_source = None
                normal_source = None
                uv_source = None
                for semantic, source, _ in inputs:
                    if semantic == "VERTEX":
                        position_source = vertices_inputs.get(f"{source}:POSITION")
                        normal_source = vertices_inputs.get(f"{source}:NORMAL")
                        uv_source = vertices_inputs.get(f"{source}:TEXCOORD")
                    elif semantic == "POSITION":
                        position_source = source
                    elif semantic == "NORMAL":
                        normal_source = source
                    elif semantic == "TEXCOORD" and uv_source is None:
                        uv_source = source

                p = read_source_tuple(sources, position_source, position_index or 0, (0.0, 0.0, 0.0))
                n = read_source_tuple(sources, normal_source, normal_index or 0, (0.0, 1.0, 0.0))
                uv = read_source_tuple(sources, uv_source, uv_index or 0, (0.0, 0.0))
                mesh.vertices.append(
                    Vertex(
                        position=(p[0], p[1], p[2]),
                        normal=normalize((n[0], n[1], n[2])),
                        uv=(uv[0], uv[1]),
                    )
                )

            submesh_indices.append(vertex_lookup[key])

        if not submesh_indices:
            continue

        if max(submesh_indices) > 65535:
            raise ValueError(f"geometry '{geom_id}' exceeds 16-bit index range")

        mesh.submeshes.append(Submesh(material=material, indices=submesh_indices))

    if not mesh.vertices or not mesh.submeshes:
        raise ValueError(f"geometry '{geom_id}' produced no renderable data")

    return mesh


def parse_dae(path: Path) -> Scene:
    root = ET.parse(path).getroot()
    material_to_texture = parse_material_textures(root)
    root_nodes = parse_visual_scene(root)
    transforms = collect_instance_transforms(root_nodes, root)
    skins = parse_skins(root)

    meshes: List[Mesh] = []
    for geometry in all_desc(root, "geometry"):
        try:
            mesh = parse_mesh_geometry(geometry, material_to_texture)
        except ValueError as exc:
            print(f"warning: {exc}", file=sys.stderr)
            continue

        geom_id = geometry.attrib.get("id") or mesh.name
        if geom_id in transforms:
            mesh.translation, mesh.rotation, mesh.scale = transforms[geom_id]
        meshes.append(mesh)

    if not meshes:
        raise ValueError("no supported mesh geometry found")

    return Scene(meshes=meshes, material_to_texture=material_to_texture, skins=skins, root_nodes=root_nodes)


def default_output_stem(path: Path) -> str:
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError:
        return path.stem

    asset = child(root, "asset")
    source_data = next(all_desc(asset, "source_data"), None) if asset is not None else None
    if source_data is not None and source_data.text:
        raw_name = source_data.text.strip().replace("\\", "/").rsplit("/", 1)[-1]
        stem = Path(raw_name).stem
        if stem:
            return stem

    return path.stem


def bounds(vertices: Sequence[Vertex]) -> Tuple[Vec3, Vec3]:
    xs = [v.position[0] for v in vertices]
    ys = [v.position[1] for v in vertices]
    zs = [v.position[2] for v in vertices]
    return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))


def collect_strings(scene: Scene) -> StringTable:
    table = StringTable()
    table.add("")
    table.add(BDAE_VERSION_STRING)
    table.add("scene")
    table.add("texture")
    table.add("texture-surface")
    table.add("unlit_textured_trans_solid")

    for texture in scene.material_to_texture.values():
        table.add(texture)

    for material in scene.material_to_texture:
        table.add(material)

    for mesh in scene.meshes:
        table.add(mesh.name)
        table.add(f"{mesh.name}-node")
        for submesh in mesh.submeshes:
            table.add(submesh.material)
            if submesh.material not in scene.material_to_texture:
                table.add(f"{submesh.material}.png")

    for skin in scene.skins:
        table.add(skin.name)
        table.add(skin.mesh_name)
        for bone_name in skin.bone_names:
            table.add(bone_name)

    def add_node_strings(node: Node) -> None:
        table.add(node.node_id)
        table.add(node.name)
        table.add(node.sid)
        for child_node in node.children:
            add_node_strings(child_node)

    for root_node in scene.root_nodes:
        add_node_strings(root_node)

    return table


def material_texture_index(materials: Sequence[str], textures: Sequence[str], scene: Scene, material: str) -> int:
    texture_name = scene.material_to_texture.get(material)
    if texture_name is None:
        texture_name = f"{material}.png"
    try:
        return textures.index(texture_name)
    except ValueError:
        return 0


def build_inner_bdae(scene: Scene) -> bytes:
    string_table = collect_strings(scene)
    header_size = 60
    offset_table_offset = header_size
    offset_count = 0
    string_table_offset = offset_table_offset
    strings_blob, string_offsets = string_table.build(string_table_offset)

    b = BinaryBuilder()
    b.write(b"\0" * header_size)
    b.write(strings_blob)
    b.align(4)

    model_info_offset = b.tell()
    model_info_patch = b.write(b"\0" * 188)

    textures: List[str] = []
    for material in scene.material_to_texture:
        texture_name = scene.material_to_texture[material]
        if texture_name not in textures:
            textures.append(texture_name)
    for mesh in scene.meshes:
        for submesh in mesh.submeshes:
            texture_name = scene.material_to_texture.get(submesh.material, f"{submesh.material}.png")
            if texture_name not in textures:
                textures.append(texture_name)

    materials = sorted({submesh.material for mesh in scene.meshes for submesh in mesh.submeshes})
    if not materials:
        materials = ["default"]

    texture_metadata_offset = b.tell()
    for texture_name in textures:
        texture_offset = string_offsets.get(texture_name)
        if texture_offset is None:
            raise ValueError(f"internal error: missing string '{texture_name}'")
        b.write(u32(string_offsets["texture"]) + u32(string_offsets["texture"]) + u32(texture_offset) + u32(0) + u32(0))

    material_metadata_offset = b.tell()
    material_patch_positions: Dict[str, int] = {}
    for material in materials:
        mat_offset = string_offsets[material]
        material_patch_positions[material] = b.tell() + 20
        b.write(
            u32(mat_offset)
            + u32(mat_offset)
            + u32(0)
            + u32(string_offsets["unlit_textured_trans_solid"])
            + u32(1)
            + u32(0)
            + u32(0)
            + u32(0)
            + u32(0)
        )

    for material in materials:
        property_offset = b.tell()
        b.patch(material_patch_positions[material], u32(property_offset))

        pointer_to_texture_pointer = b.tell() + 24
        texture_pointer = b.tell() + 28
        texture_index = material_texture_index(materials, textures, scene, material)
        b.write(
            u32(string_offsets["texture"])
            + u32(string_offsets["texture"])
            + u32(11)
            + u32(1)
            + u32(0)
            + u32(pointer_to_texture_pointer)
            + u32(texture_pointer)
            + u32(texture_index)
        )

    mesh_metadata_offset = b.tell()
    mesh_data_patches: List[int] = []
    for mesh in scene.meshes:
        name_offset = string_offsets[mesh.name]
        b.write(u32(name_offset) + u32(name_offset) + u32(0))
        mesh_data_patches.append(b.tell())
        b.write(u32(0))

    for mesh, patch_offset in zip(scene.meshes, mesh_data_patches):
        mesh_data_offset = b.tell()
        b.patch(patch_offset, u32(mesh_data_offset))

        submesh_data_offset = mesh_data_offset + 84
        vertex_data_patch = mesh_data_offset + 80
        bbox_min, bbox_max = bounds(mesh.vertices)

        b.write(
            u32(1)
            + u32(len(mesh.vertices))
            + u32(40)
            + u32(len(mesh.submeshes))
            + u32(submesh_data_offset)
            + vec3(bbox_min)
            + vec3(bbox_max)
            + u32(32)
            + (u32(0) * 8)
            + u32(0)
        )

        submesh_index_patches: List[int] = []
        for submesh in mesh.submeshes:
            material_offset = string_offsets[submesh.material]
            indices = submesh.indices
            b.write(u32(material_offset))
            b.write(u32(material_offset))
            b.write(s32(0) * 5)
            b.write(u32(min(indices)))
            b.write(u32(max(indices)))
            b.write(u32(0))
            b.write(u32(len(indices)))
            submesh_index_patches.append(b.tell())
            b.write(u32(0))
            b.write(u32(0) + u32(0))

        b.align(4)
        vertex_data_offset = b.tell()
        b.patch(vertex_data_patch, u32(vertex_data_offset))
        b.write(u32(0))
        for vertex in mesh.vertices:
            b.write(vec3(vertex.position) + vec3(vertex.normal) + vec2(vertex.uv))

        for submesh, index_patch in zip(mesh.submeshes, submesh_index_patches):
            b.align(4)
            index_data_offset = b.tell()
            b.patch(index_patch, u32(index_data_offset))
            b.write(u32(0))
            for index in submesh.indices:
                b.write(u16(index))
            b.align(4)

    mesh_skin_metadata_offset = b.tell()
    skin_data_patches: List[int] = []
    for skin in scene.skins:
        b.write(u32(0) + u32(string_offsets[skin.name]))
        skin_data_patches.append(b.tell())
        b.write(u32(0))

    for skin, patch_offset in zip(scene.skins, skin_data_patches):
        b.align(4)
        skin_data_offset = b.tell()
        b.patch(patch_offset, u32(skin_data_offset))

        bind_pose_data_patch = skin_data_offset + 4
        dual_quat_data_patch = skin_data_offset + 12
        bone_names_patch = skin_data_offset + 120
        influence_data_patch = skin_data_offset + 128
        unknown_data_patch = skin_data_offset + 136
        bbox_data_patch = skin_data_offset + 144

        bone_count = len(skin.bone_names)
        dual_quat_count = bone_count * 8
        influence_float_count = len(skin.influences) * (skin.max_influence + 1)

        bind_shape = list(skin.bind_shape_matrix[:16])
        bind_shape += [0.0] * (24 - len(bind_shape))

        b.write(
            u32(bone_count * 16)
            + u32(0)
            + u32(dual_quat_count)
            + u32(0)
            + b"".join(f32(value) for value in bind_shape[:24])
            + u32(string_offsets[skin.mesh_name])
            + u32(bone_count)
            + u32(0)
            + u32(influence_float_count)
            + u32(0)
            + u32(bone_count)
            + u32(0)
            + u32(len(skin.bounding_boxes))
            + u32(0)
            + u32(0)
            + u32(skin.max_influence)
        )

        b.align(4)
        bind_pose_data_offset = b.tell()
        b.patch(bind_pose_data_patch, u32(bind_pose_data_offset))
        for matrix in skin.inverse_bind_matrices:
            b.write(b"".join(f32(value) for value in matrix[:16]))

        b.align(4)
        dual_quat_data_offset = b.tell()
        b.patch(dual_quat_data_patch, u32(dual_quat_data_offset))
        for _ in range(dual_quat_count):
            b.write(f32(0.0))

        b.align(4)
        bone_names_offset = b.tell()
        b.patch(bone_names_patch, u32(bone_names_offset))
        for bone_name in skin.bone_names:
            b.write(u32(string_offsets[bone_name]))

        b.align(4)
        bbox_data_offset = b.tell()
        b.patch(bbox_data_patch, u32(bbox_data_offset))
        for bbox_min, bbox_max in skin.bounding_boxes:
            b.write(vec3(bbox_min) + vec3(bbox_max))

        b.align(4)
        influence_data_offset = b.tell()
        b.patch(influence_data_patch, u32(influence_data_offset))
        b.write(u32(0))
        for vertex_influences in skin.influences:
            limited = vertex_influences[: skin.max_influence]
            bone_indices = [bone_index for bone_index, _ in limited]
            weights = [weight for _, weight in limited]
            bone_indices += [0] * (4 - len(bone_indices))
            weights += [0.0] * (skin.max_influence - len(weights))
            b.write(bytes(bone_indices[:4]))
            for weight in weights[: skin.max_influence]:
                b.write(f32(weight))

        b.align(4)
        unknown_data_offset = b.tell()
        b.patch(unknown_data_patch, u32(unknown_data_offset))
        b.write(u32(0) * max(bone_count, 1))

    root_nodes = scene.root_nodes
    if not root_nodes:
        root_nodes = [
            Node(
                node_id=f"{mesh.name}-node",
                name=mesh.name,
                sid="",
                translation=mesh.translation,
                rotation=mesh.rotation,
                scale=mesh.scale,
            )
            for mesh in scene.meshes
        ]

    node_tree_metadata_offset = b.tell()
    node_tree_data_patch = b.tell() + 12
    b.write(
        u32(string_offsets["scene"])
        + u32(string_offsets["scene"])
        + u32(len(root_nodes))
        + u32(0)
    )

    def write_node_block(nodes: Sequence[Node]) -> int:
        block_offset = b.tell()
        child_patches: List[Tuple[int, Sequence[Node]]] = []
        empty_offset = string_offsets[""]

        for node in nodes:
            node_id_offset = string_offsets.get(node.node_id, empty_offset)
            name_offset = string_offsets.get(node.name, empty_offset)
            sid_offset = string_offsets.get(node.sid, empty_offset)
            child_count = len(node.children)

            b.write(
                u32(node_id_offset)
                + u32(name_offset)
                + u32(sid_offset)
                + vec3(node.translation)
                + quat(node.rotation)
                + vec3(node.scale)
                + u32(1)
                + u32(child_count)
            )
            child_offset_patch = b.tell()
            b.write(
                u32(0)
                + u32(0)
                + u32(0)
                + u32(0)
                + u32(0)
            )

            if child_count:
                child_patches.append((child_offset_patch, node.children))

        for child_offset_patch, child_nodes in child_patches:
            child_offset = write_node_block(child_nodes)
            b.patch(child_offset_patch, u32(child_offset))

        return block_offset

    node_tree_data_offset = write_node_block(root_nodes)
    b.patch(node_tree_data_patch, u32(node_tree_data_offset))

    removable_offset = b.tell()
    size_removable = 0
    dynamic_size = 0
    file_size = b.tell()

    model_info = bytearray()
    model_info += u32(string_offsets[BDAE_VERSION_STRING])
    model_info += u32(0) * 6
    model_info += s32(0)
    model_info += s32(0)
    model_info += u32(0)
    model_info += u32(0)
    model_info += u32(0) * 8
    model_info += u32(len(textures))
    model_info += u32(texture_metadata_offset)
    model_info += u32(0) + u32(0)
    model_info += u32(len(materials))
    model_info += u32(material_metadata_offset)
    model_info += u32(len(scene.meshes))
    model_info += u32(mesh_metadata_offset)
    model_info += u32(len(scene.skins))
    model_info += u32(mesh_skin_metadata_offset)
    model_info += (u32(0) + u32(0)) * 4
    model_info += u32(1)
    model_info += u32(node_tree_metadata_offset)
    model_info += (u32(0) + u32(0)) * 4
    assert len(model_info) == 188
    b.patch(model_info_patch, bytes(model_info))

    header = bytearray()
    header += b"BRES"
    header += struct.pack("<H", 0xFFFE)
    header += u16(0)
    header += u32(header_size)
    header += u32(file_size)
    header += u32(offset_count)
    header += u32(0)
    header += u32(offset_table_offset)
    header += u32(string_table_offset)
    header += u32(model_info_offset)
    header += u32(0)
    header += u32(removable_offset)
    header += u32(size_removable)
    header += u32(0)
    header += u32(0)
    header += u32(dynamic_size)
    assert len(header) == header_size
    b.patch(0, bytes(header))

    return bytes(b.data)


def write_outer_bdae(inner: bytes, output: Path, source: Optional[Path]) -> None:
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as archive:
        archive.writestr(INNER_BDAE_NAME, inner)
        archive.writestr("version.nfo", BDAE_VERSION_STRING)
        if source is not None:
            archive.write(source, "source.dae")


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Convert static COLLADA geometry to 32-bit OaC BDAE.")
    parser.add_argument("input", type=Path, help="input .dae file")
    parser.add_argument("-o", "--output", type=Path, help="output .bdae file")
    parser.add_argument("--inner", action="store_true", help="write raw inner BDAE instead of outer ZIP package")
    parser.add_argument("--no-source", action="store_true", help="do not include source.dae in the outer archive")
    args = parser.parse_args(argv)

    input_path = args.input
    if input_path.suffix.lower() != ".dae":
        parser.error("input must be a .dae file")
    if not input_path.exists():
        parser.error(f"input not found: {input_path}")

    output_path = args.output
    if output_path is None:
        output_path = input_path.with_name(default_output_stem(input_path)).with_suffix(
            ".inner.bdae" if args.inner else ".bdae"
        )

    scene = parse_dae(input_path)
    inner = build_inner_bdae(scene)

    if args.inner:
        output_path.write_bytes(inner)
    else:
        write_outer_bdae(inner, output_path, None if args.no_source else input_path)

    vertex_count = sum(len(mesh.vertices) for mesh in scene.meshes)
    face_count = sum(len(submesh.indices) // 3 for mesh in scene.meshes for submesh in mesh.submeshes)
    print(
        f"wrote {output_path} ({len(scene.meshes)} mesh(es), "
        f"{vertex_count} vertices, {face_count} triangles)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
