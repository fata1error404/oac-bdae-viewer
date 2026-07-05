#!/usr/bin/env python3
"""Convert a small static COLLADA subset to OaC 32-bit BDAE.

This is an intentionally narrow first pass. It writes an outer .bdae ZIP
containing a 32-bit little_endian_not_quantized.bdae payload compatible with
the old 0.0.0.779 layout documented by aux_docs/BDAE_model_1.3.0.bt.
"""

from __future__ import annotations

import argparse
import datetime
import math
import struct
import sys
import tempfile
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple
from xml.etree import ElementTree as ET


BDAE_VERSION_STRING = "0,0,0,779"
INNER_BDAE_NAME = "little_endian_not_quantized.bdae"


Vec2 = Tuple[float, float]
Vec3 = Tuple[float, float, float]
Color = Tuple[int, int, int, int]
Quat = Tuple[float, float, float, float]
SetParam = Tuple[str, str, int, int]


@dataclass
class Vertex:
    position: Vec3
    normal: Vec3
    uv: Vec2
    color: Optional[Color] = None


@dataclass
class Submesh:
    material: str
    indices: List[int]


@dataclass
class Mesh:
    name: str
    mesh_id: str = ""
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
class Instance:
    type_id: int
    url: str
    material_target: str = ""
    skeleton: str = ""


@dataclass
class Node:
    node_id: str
    name: str
    sid: str
    translation: Vec3 = (0.0, 0.0, 0.0)
    rotation: Quat = (0.0, 0.0, 0.0, 1.0)
    scale: Vec3 = (1.0, 1.0, 1.0)
    instances: List[Instance] = field(default_factory=list)
    children: List["Node"] = field(default_factory=list)


@dataclass
class ImageInfo:
    image_id: str
    name: str
    path: str


@dataclass
class MaterialInfo:
    material_id: str
    name: str
    effect_file: str
    effect_ref: str
    setparams: List[SetParam]


@dataclass
class EffectInfo:
    effect_id: str
    name: str
    profile_id: str
    profile_name: str


@dataclass
class Scene:
    meshes: List[Mesh]
    material_to_texture: Dict[str, str]
    skins: List[Skin] = field(default_factory=list)
    root_nodes: List[Node] = field(default_factory=list)
    extra_strings: List[str] = field(default_factory=list)
    node_tree_id: str = "VisualSceneNode"
    node_tree_name: str = "VisualSceneNode"
    images: List[ImageInfo] = field(default_factory=list)
    materials: List[MaterialInfo] = field(default_factory=list)
    effects: List[EffectInfo] = field(default_factory=list)


class StringTable:
    def __init__(self) -> None:
        self._offsets: Dict[str, int] = {}
        self._items: List[str] = []

    def add(self, value: str) -> int:
        if value not in self._offsets:
            self._offsets[value] = -1
            self._items.append(value)
        return self._offsets[value]

    def add_duplicate(self, value: str) -> None:
        self._items.append(value)

    def build(self, start_offset: int) -> Tuple[bytes, Dict[str, int]]:
        out = bytearray()
        offsets: Dict[str, int] = {}

        for value in self._items:
            encoded = value.encode("utf-8")
            out += u32(len(encoded))
            if value not in offsets:
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


@dataclass
class PendingRemovableChunk:
    type_id: int
    payload: bytearray
    offset_patches: List[Tuple[int, int]] = field(default_factory=list)


def u16(value: int) -> bytes:
    return struct.pack("<H", value)


def s32(value: int) -> bytes:
    return struct.pack("<i", value)


def u32(value: int) -> bytes:
    return struct.pack("<I", value)


def f32(value: float) -> bytes:
    return struct.pack("<f", value)


def to_f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def clean_f32(value: float) -> float:
    value = to_f32(value)
    return 0.0 if value == 0.0 else value


def float_to_u32(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def u32_to_float(value: int) -> float:
    return struct.unpack("<f", struct.pack("<I", value & 0xFFFFFFFF))[0]


def reciprocal_squareroot(value: float) -> float:
    x = to_f32(value)
    tmp = (((0x3F800000 << 1) + 0x3F800000 - float_to_u32(x)) & 0xFFFFFFFF) >> 1
    y = u32_to_float(tmp)
    return to_f32(y * (1.47 - 0.47 * x * y * y))


def reciprocal_squareroot_extended(value: float) -> float:
    x = to_f32(value)
    tmp = (((0x3F800000 << 1) + 0x3F800000 - float_to_u32(x)) & 0xFFFFFFFF) >> 1
    y = u32_to_float(tmp)
    return y * (1.47 - 0.47 * x * y * y)


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
    deg_to_rad = to_f32(to_f32(3.14159265359) / to_f32(180.0))
    half = to_f32(to_f32(to_f32(angle_degrees) * deg_to_rad) * to_f32(0.5))
    s = to_f32(math.sin(half))
    q = (
        to_f32(to_f32(axis[0]) * s),
        to_f32(to_f32(axis[1]) * s),
        to_f32(to_f32(axis[2]) * s),
        to_f32(math.cos(half)),
    )
    return quat_normalize_axis_angle(q)


def quat_normalize_axis_angle(q: Quat) -> Quat:
    x, y, z, w = (to_f32(value) for value in q)
    n = to_f32(x * x + y * y + z * z + w * w)
    if n != 1.0 and n != 0.0:
        inv = reciprocal_squareroot_extended(n)
        x, y, z, w = (to_f32(value * inv) for value in (x, y, z, w))
    return tuple(0.0 if value == 0.0 else value for value in (x, y, z, w))  # type: ignore[return-value]


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


def quat_normalize(q: Quat) -> Quat:
    x, y, z, w = q
    x, y, z, w = to_f32(x), to_f32(y), to_f32(z), to_f32(w)
    n = to_f32(to_f32(to_f32(x * x) + to_f32(y * y)) + to_f32(to_f32(z * z) + to_f32(w * w)))
    if n == 1.0 or n == 0.0:
        return q
    inv = reciprocal_squareroot(n)
    return (to_f32(x * inv), to_f32(y * inv), to_f32(z * inv), to_f32(w * inv))


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


def pack_color(values: Sequence[float]) -> Color:
    components = list(values[:4])
    components += [1.0] * (4 - len(components))
    return tuple(max(0, min(255, int(to_f32(value * 255.0) + 0.5))) for value in components[:4])  # type: ignore[return-value]


def parse_material_textures(root: ET.Element) -> Dict[str, str]:
    images: Dict[str, str] = {}
    for image in all_desc(root, "image"):
        image_id = image.attrib.get("id")
        init_from = child(image, "init_from")
        if image_id and init_from is not None and init_from.text:
            images[image_id] = init_from.text.strip()

    effect_textures: Dict[str, str] = {}
    for effect in all_desc(root, "effect"):
        effect_id = effect.attrib.get("id")
        if not effect_id:
            continue

        texture_name = ""
        for init_from in all_desc(effect, "init_from"):
            if init_from.text:
                ref = clean_id(init_from.text.strip())
                texture_name = images.get(ref, ref)
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
                    texture_name = images.get(ref, ref)
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


def parse_images(root: ET.Element) -> List[ImageInfo]:
    images: List[ImageInfo] = []
    for image in all_desc(root, "image"):
        image_id = image.attrib.get("id", "")
        image_name = image.attrib.get("name", image_id)
        init_from = child(image, "init_from")
        path = init_from.text.strip() if init_from is not None and init_from.text else image_name
        images.append(ImageInfo(image_id=image_id, name=image_name, path=path))
    return images


def sampler_texture_indices(container: ET.Element, image_indices: Dict[str, int]) -> Dict[str, int]:
    surfaces: Dict[str, int] = {}
    sampler_sources: Dict[str, str] = {}

    for elem in all_desc(container, "setparam"):
        sid = elem.attrib.get("ref", "")
        surface = child(elem, "surface")
        if sid and surface is not None:
            init_from = child(surface, "init_from")
            if init_from is not None and init_from.text:
                image_id = clean_id(init_from.text.strip())
                if image_id in image_indices:
                    surfaces[sid] = image_indices[image_id]

        sampler = child(elem, "sampler2D")
        if sid and sampler is not None:
            source = child(sampler, "source")
            if source is not None and source.text:
                sampler_sources[sid] = source.text.strip()

    for elem in all_desc(container, "newparam"):
        sid = elem.attrib.get("sid", "")
        surface = child(elem, "surface")
        if sid and surface is not None:
            init_from = child(surface, "init_from")
            if init_from is not None and init_from.text:
                image_id = clean_id(init_from.text.strip())
                if image_id in image_indices:
                    surfaces[sid] = image_indices[image_id]

        sampler = child(elem, "sampler2D")
        if sid and sampler is not None:
            source = child(sampler, "source")
            if source is not None and source.text:
                sampler_sources[sid] = source.text.strip()

    return {
        sampler_ref: surfaces[surface_ref]
        for sampler_ref, surface_ref in sampler_sources.items()
        if surface_ref in surfaces
    }


def profile_common_sampler_setparams(root: ET.Element, image_indices: Dict[str, int]) -> Dict[str, List[SetParam]]:
    values: Dict[str, List[SetParam]] = {}
    for effect in all_desc(root, "effect"):
        effect_id = effect.attrib.get("id", "")
        profile_common = child(effect, "profile_COMMON")
        if not effect_id or profile_common is None:
            continue
        values[effect_id] = [
            (sampler_ref, "", 11, texture_index)
            for sampler_ref, texture_index in sampler_texture_indices(profile_common, image_indices).items()
        ]
    return values


def parse_material_infos(root: ET.Element, image_indices: Dict[str, int]) -> List[MaterialInfo]:
    profile_samplers = profile_common_sampler_setparams(root, image_indices)
    materials: List[MaterialInfo] = []
    for material in all_desc(root, "material"):
        material_id = material.attrib.get("id") or material.attrib.get("name", "")
        material_name = material.attrib.get("name", material_id)
        effect_file = ""
        effect_ref = ""
        raw_effect_ref = ""
        instance_effect = child(material, "instance_effect")
        if instance_effect is not None:
            url = instance_effect.attrib.get("url", "")
            if "#" in url and not url.startswith("#"):
                effect_file, raw_ref = url.split("#", 1)
                effect_ref = f"#{raw_ref}"
                raw_effect_ref = raw_ref
            else:
                effect_ref = url
                raw_effect_ref = clean_id(url)

        setparams: List[SetParam] = []
        if instance_effect is not None:
            texture_indices = sampler_texture_indices(instance_effect, image_indices)
            for setparam in all_desc(instance_effect, "setparam"):
                ref = setparam.attrib.get("ref", "")
                if child(setparam, "sampler2D") is not None:
                    setparams.append((ref, "", 11, texture_indices.get(ref, 0)))
                elif "CurrentTechnique" in ref:
                    sid_elem = child(setparam, "string")
                    setparams.append((ref, sid_elem.text.strip() if sid_elem is not None and sid_elem.text else "", 20, 0))

        if not setparams and raw_effect_ref in profile_samplers:
            setparams.extend(profile_samplers[raw_effect_ref])

        materials.append(
            MaterialInfo(
                material_id=material_id,
                name=material_name,
                effect_file=effect_file,
                effect_ref=effect_ref,
                setparams=setparams,
            )
        )
    return materials


PROFILE_COMMON_STRINGS = [
    "default",
    "fog",
    "lighting",
    "fog+lighting",
    "diffuse-sampler",
    "diffuse-sampler-matrix",
    "ambient-color",
    "diffuse-color",
    "specular-color",
    "emission-color",
    "shininess",
    "ProfileCOMMON_emul_VS.glsl",
    "main",
    "#define TEXTURED\n",
    "ProfileCOMMON_emul_FS.glsl",
    "DiffuseColor",
    "Sampler0",
    "TextureMatrix0",
    "#define TEXTURED\n#define FOG\n ",
    "#define TEXTURED\n#define LIGHTING\n",
    "ambientcolor",
    "specularcolor",
    "emissioncolor",
    "fog & lighting",
    "#define TEXTURED\n#define FOG\n #define LIGHTING\n",
    "skinning",
    "#define TEXTURED\n#define SKINNED\n",
    "BoneMatrices",
    "WeightMask",
    "skinning quat",
    "#define TEXTURED\n#define QUATSKINNED\n",
    "BoneQuat0",
    "BoneQuat1",
    "skinning texture",
    "#define TEXTURED\n#define TEXTURESKINNED\n",
    "BoneTexture",
    "BoneTextureParams",
    "weight-mask",
    "bone-matrices",
    "bone-quat0",
    "bone-quat1",
    "bone-texture-sampler",
    "bone-texture-params",
    "ProfileCOMMON_emul.cg",
    "VS",
    "-DTEXTURED ",
    "FS",
    "-DTEXTURED -DFOG ",
    "-DTEXTURED -DLIGHTING ",
    "-DTEXTURED -DFOG -DLIGHTING ",
    "-DTEXTURED -DSKINNED ",
    "-DTEXTURED -DQUATSKINNED ",
    "-DTEXTURED -DTEXTURESKINNED ",
    "ProfileCOMMON_emul.hlsl",
    "pass0",
]

PROFILE_COMMON_EFFECT_SIZE = 26596
PROFILE_COMMON_EFFECT_RELOCATION_COUNT = 677


def profile_common_suffix(root: ET.Element) -> str:
    asset = child(root, "asset")
    modified = child(asset, "modified") if asset is not None else None
    if modified is not None and modified.text:
        try:
            dt = datetime.datetime.fromisoformat(modified.text.strip().replace("Z", "+00:00"))
            return f"{int(dt.timestamp()) + 1}_bdae_export_tmp"
        except ValueError:
            pass
    return "0_bdae_export_tmp"


def parse_effect_infos(root: ET.Element) -> List[EffectInfo]:
    effects: List[EffectInfo] = []
    suffix = profile_common_suffix(root)
    for effect in all_desc(root, "effect"):
        effect_id = effect.attrib.get("id", "")
        effect_name = effect.attrib.get("name", effect_id)
        if child(effect, "profile_COMMON") is None:
            continue
        profile_id = f"ProfileCOMMON_{effect_id}{suffix}"
        effects.append(
            EffectInfo(
                effect_id=effect_id,
                name=effect_name,
                profile_id=profile_id,
                profile_name=f"ProfileCOMMON_{effect_name}",
            )
        )
    return effects


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
            rotation = quat_normalize(quat_from_axis_angle((values[0], values[1], values[2]), -values[3]))
        elif name == "matrix":
            saw_matrix = True

    if saw_matrix:
        print(
            f"warning: node '{elem.attrib.get('id', elem.attrib.get('name', ''))}' "
            "uses matrix transform; dae2bdae.py currently writes identity/TRS only",
            file=sys.stderr,
        )

    instances: List[Instance] = []
    for instance_camera in children(elem, "instance_camera"):
        instances.append(Instance(type_id=1, url=instance_camera.attrib.get("url", "")))
    for instance_geometry in children(elem, "instance_geometry"):
        material_target = ""
        material_elem = next(all_desc(instance_geometry, "instance_material"), None)
        if material_elem is not None:
            material_target = material_elem.attrib.get("target", "")
        instances.append(
            Instance(
                type_id=3,
                url=instance_geometry.attrib.get("url", ""),
                material_target=material_target,
            )
        )
    for instance_controller in children(elem, "instance_controller"):
        material_target = ""
        skeleton = ""
        material_elem = next(all_desc(instance_controller, "instance_material"), None)
        if material_elem is not None:
            material_target = material_elem.attrib.get("target", "")
        skeleton_elem = next(all_desc(instance_controller, "skeleton"), None)
        if skeleton_elem is not None and skeleton_elem.text:
            skeleton = skeleton_elem.text.strip()
        instances.append(
            Instance(
                type_id=2,
                url=instance_controller.attrib.get("url", ""),
                material_target=material_target,
                skeleton=skeleton,
            )
        )
    for instance_light in children(elem, "instance_light"):
        instances.append(Instance(type_id=4, url=instance_light.attrib.get("url", "")))

    return Node(
        node_id=elem.attrib.get("id", elem.attrib.get("name", "")),
        name=elem.attrib.get("name", ""),
        sid=elem.attrib.get("sid", ""),
        translation=translation,
        rotation=rotation,
        scale=scale,
        instances=instances,
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


def visual_scene_metadata(root: ET.Element) -> Tuple[str, str]:
    scene = child(root, "scene")
    visual_scene_id = ""

    if scene is not None:
        instance_visual_scene = child(scene, "instance_visual_scene")
        if instance_visual_scene is not None:
            visual_scene_id = clean_id(instance_visual_scene.attrib.get("url", ""))

    for candidate in all_desc(root, "visual_scene"):
        if not visual_scene_id or candidate.attrib.get("id") == visual_scene_id:
            node_tree_id = candidate.attrib.get("id", "VisualSceneNode")
            node_tree_name = candidate.attrib.get("name", node_tree_id)
            return node_tree_id, node_tree_name

    return "VisualSceneNode", "VisualSceneNode"


def legacy_reference_strings(root: ET.Element) -> List[str]:
    values: List[str] = []
    effect_infos = parse_effect_infos(root)
    effect_ref_map = {f"#{effect.effect_id}": f"#{effect.profile_id}" for effect in effect_infos}

    def add(value: Optional[str]) -> None:
        if value is not None:
            values.append(value.strip())

    def add_raw(value: Optional[str]) -> None:
        if value is not None:
            values.append(value)

    def add_url(value: Optional[str]) -> None:
        if not value:
            return
        url = value.strip()
        if "#" in url and not url.startswith("#"):
            before, after = url.split("#", 1)
            add(before)
            add(f"#{after}")
        else:
            add(url)

    for image in all_desc(root, "image"):
        add(image.attrib.get("id"))
        add(image.attrib.get("name"))
        init_from = child(image, "init_from")
        if init_from is not None and init_from.text:
            add(init_from.text.strip())

    for effect in effect_infos:
        add(effect.profile_id)
        add(effect.profile_name)
        for value in PROFILE_COMMON_STRINGS:
            add_raw(value)

    for material in all_desc(root, "material"):
        add(material.attrib.get("id") or material.attrib.get("name"))
        instance_effect = child(material, "instance_effect")
        if instance_effect is not None:
            url = instance_effect.attrib.get("url")
            if url in effect_ref_map:
                add_url(effect_ref_map[url])
            else:
                add_url(url)

    add("")
    add("")

    for setparam in all_desc(root, "setparam"):
        ref = setparam.attrib.get("ref")
        if ref and child(setparam, "sampler2D") is not None:
            add(ref)
        elif ref and "CurrentTechnique" in ref:
            add(ref)
            string_value = child(setparam, "string")
            add(string_value.text if string_value is not None else None)

    for geometry in all_desc(root, "geometry"):
        add(geometry.attrib.get("id"))
        add(geometry.attrib.get("name"))

    for controller in all_desc(root, "controller"):
        add(controller.attrib.get("id"))
        skin_elem = child(controller, "skin")
        if skin_elem is not None:
            add(skin_elem.attrib.get("source"))
            for source in children(skin_elem, "source"):
                if child(source, "Name_array") is not None:
                    for bone_name in source_names(source):
                        add(bone_name)
                    break

    node_tree_id, node_tree_name = visual_scene_metadata(root)
    add(node_tree_id)
    add(node_tree_name)

    def add_node_and_instances(node_elem: ET.Element) -> None:
        add(node_elem.attrib.get("id"))
        add(node_elem.attrib.get("name"))
        add(node_elem.attrib.get("sid"))

        for child_node in children(node_elem, "node"):
            add_node_and_instances(child_node)

        for instance_camera in children(node_elem, "instance_camera"):
            add_url(instance_camera.attrib.get("url"))
        for instance_geometry in children(node_elem, "instance_geometry"):
            add_url(instance_geometry.attrib.get("url"))
            for instance_material in all_desc(instance_geometry, "instance_material"):
                add_url(instance_material.attrib.get("target"))
        for instance_controller in children(node_elem, "instance_controller"):
            add_url(instance_controller.attrib.get("url"))
            for instance_material in all_desc(instance_controller, "instance_material"):
                add_url(instance_material.attrib.get("target"))
            for skeleton in all_desc(instance_controller, "skeleton"):
                add_url(skeleton.text)
        for instance_light in children(node_elem, "instance_light"):
            add_url(instance_light.attrib.get("url"))

    visual_scene = None
    for candidate in all_desc(root, "visual_scene"):
        if candidate.attrib.get("id") == node_tree_id:
            visual_scene = candidate
            break
    if visual_scene is not None:
        for node_elem in children(visual_scene, "node"):
            add_node_and_instances(node_elem)

    scene = child(root, "scene")
    if scene is not None:
        instance_visual_scene = child(scene, "instance_visual_scene")
        if instance_visual_scene is not None:
            add_url(instance_visual_scene.attrib.get("url"))

    return values


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

    mesh = Mesh(name=geom_name, mesh_id=geom_id)
    vertex_lookup: Dict[Tuple[Optional[int], Optional[int], Optional[int], Optional[int]], int] = {}

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
            color_index: Optional[int] = None

            for semantic, source, offset in inputs:
                index = index_tuple[offset]
                if semantic == "VERTEX":
                    position_index = index
                    normal_index = index
                    uv_index = index
                    color_index = index
                    if f"{source}:POSITION" in vertices_inputs:
                        position_index = index
                    if f"{source}:NORMAL" not in vertices_inputs:
                        normal_index = None
                    if f"{source}:TEXCOORD" not in vertices_inputs:
                        uv_index = None
                    if f"{source}:COLOR" not in vertices_inputs:
                        color_index = None
                elif semantic == "POSITION":
                    position_index = index
                elif semantic == "NORMAL":
                    normal_index = index
                elif semantic == "TEXCOORD" and uv_index is None:
                    uv_index = index
                elif semantic == "COLOR" and color_index is None:
                    color_index = index

            key = (position_index, normal_index, uv_index, color_index)
            if key not in vertex_lookup:
                vertex_lookup[key] = len(mesh.vertices)

                position_source = None
                normal_source = None
                uv_source = None
                color_source = None
                for semantic, source, _ in inputs:
                    if semantic == "VERTEX":
                        position_source = vertices_inputs.get(f"{source}:POSITION")
                        normal_source = vertices_inputs.get(f"{source}:NORMAL")
                        uv_source = vertices_inputs.get(f"{source}:TEXCOORD")
                        color_source = vertices_inputs.get(f"{source}:COLOR")
                    elif semantic == "POSITION":
                        position_source = source
                    elif semantic == "NORMAL":
                        normal_source = source
                    elif semantic == "TEXCOORD" and uv_source is None:
                        uv_source = source
                    elif semantic == "COLOR" and color_source is None:
                        color_source = source

                p = read_source_tuple(sources, position_source, position_index or 0, (0.0, 0.0, 0.0))
                n = read_source_tuple(sources, normal_source, normal_index or 0, (0.0, 1.0, 0.0))
                uv = read_source_tuple(sources, uv_source, uv_index or 0, (0.0, 0.0))
                color = None
                if color_source is not None and color_index is not None:
                    color = pack_color(read_source_tuple(sources, color_source, color_index, (1.0, 1.0, 1.0, 1.0)))
                mesh.vertices.append(
                    Vertex(
                        position=(p[0], p[1], p[2]),
                        normal=(n[0], n[1], n[2]),
                        uv=(uv[0], uv[1]),
                        color=color,
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
    effects = parse_effect_infos(root)
    effect_ref_map = {f"#{effect.effect_id}": f"#{effect.profile_id}" for effect in effects}
    material_to_texture = parse_material_textures(root)
    images = parse_images(root)
    image_indices = {image.image_id: index for index, image in enumerate(images)}
    material_infos = parse_material_infos(root, image_indices)
    for material_info in material_infos:
        if material_info.effect_ref in effect_ref_map:
            material_info.effect_file = ""
            material_info.effect_ref = effect_ref_map[material_info.effect_ref]
    root_nodes = parse_visual_scene(root)
    node_tree_id, node_tree_name = visual_scene_metadata(root)
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

    return Scene(
        meshes=meshes,
        material_to_texture=material_to_texture,
        skins=skins,
        root_nodes=root_nodes,
        extra_strings=legacy_reference_strings(root),
        node_tree_id=node_tree_id,
        node_tree_name=node_tree_name,
        images=images,
        materials=material_infos,
        effects=effects,
    )


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
    table.add(BDAE_VERSION_STRING)

    empty_seen = False
    for value in scene.extra_strings:
        if value == "":
            if empty_seen:
                table.add_duplicate(value)
            else:
                table.add(value)
                empty_seen = True
        else:
            table.add(value)

    if not scene.images:
        table.add("texture")
    table.add(scene.node_tree_id)
    table.add(scene.node_tree_name)

    for texture in scene.material_to_texture.values():
        table.add(texture)

    for material in scene.material_to_texture:
        table.add(material)

    for mesh in scene.meshes:
        if mesh.mesh_id:
            table.add(mesh.mesh_id)
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


def aligned_payload(payload: bytearray) -> bytearray:
    while len(payload) % 4:
        payload.append(0)
    return payload


def normalized_influences(vertex_influences: Sequence[Tuple[int, float]], max_influence: int) -> Tuple[List[int], List[float]]:
    limited = sorted(vertex_influences, key=lambda item: item[1], reverse=True)[:max_influence]
    weight_sum = sum(weight for _, weight in limited)
    scale = (1.0 / weight_sum) if weight_sum else 0.0

    bone_indices = [bone_index for bone_index, _ in limited]
    weights = [weight * scale for _, weight in limited]
    bone_indices += [0] * (4 - len(bone_indices))
    weights += [0.0] * (max_influence - len(weights))
    return bone_indices[:4], weights[:max_influence]


def vertex_payload(mesh: Mesh) -> bytearray:
    payload = bytearray()
    has_color = any(vertex.color is not None for vertex in mesh.vertices)
    for vertex in mesh.vertices:
        payload += vec3(vertex.position) + vec3(vertex.normal) + vec2(vertex.uv)
        if has_color:
            payload += bytes(vertex.color or (255, 255, 255, 255))
    return aligned_payload(payload)


def index_payload(submesh: Submesh) -> bytearray:
    payload = bytearray()
    for index in submesh.indices:
        payload += u16(index)
    return aligned_payload(payload)


def influence_payload(skin: Skin) -> bytearray:
    payload = bytearray()
    for vertex_influences in skin.influences:
        bone_indices, weights = normalized_influences(vertex_influences, skin.max_influence)
        payload += bytes(bone_indices)
        for weight in weights:
            payload += f32(weight)

    entry_size = 4 + skin.max_influence * 4
    data_count = len(skin.influences) * (skin.max_influence + 1)
    payload += b"\0" * max(0, (data_count - len(skin.influences)) * entry_size)
    return aligned_payload(payload)


def optimized_skin_payload(skin: Skin) -> PendingRemovableChunk:
    per_bone: List[List[Tuple[float, int]]] = [[] for _ in skin.bone_names]
    for vertex_index, vertex_influences in enumerate(skin.influences):
        bone_indices, weights = normalized_influences(vertex_influences, skin.max_influence)
        for bone_index, weight in zip(bone_indices, weights):
            if weight == 0.0 or bone_index >= len(per_bone):
                continue
            per_bone[bone_index].append((weight, vertex_index))

    payload = bytearray()
    patches: List[Tuple[int, int]] = []
    child_payloads: List[bytearray] = []

    for entries in per_bone:
        payload += u32(len(entries))
        patch_offset = len(payload)
        payload += u32(0)

        child = bytearray()
        for weight, vertex_index in entries:
            child += f32(weight) + u16(vertex_index) + u16(0)
        child_payloads.append(aligned_payload(child))
        patches.append((patch_offset, 0))

    for index, child in enumerate(child_payloads):
        child_offset = len(payload)
        payload += child
        patches[index] = (patches[index][0], child_offset)

    return PendingRemovableChunk(2, aligned_payload(payload), patches)


def matrix_to_quat(m: Sequence[float]) -> Quat:
    r00, r01, r02 = to_f32(m[0]), to_f32(m[1]), to_f32(m[2])
    r10, r11, r12 = to_f32(m[4]), to_f32(m[5]), to_f32(m[6])
    r20, r21, r22 = to_f32(m[8]), to_f32(m[9]), to_f32(m[10])
    diag = to_f32(r00 + r11 + r22)
    if diag > 0.0:
        scale = to_f32(math.sqrt(to_f32(1.0 + diag)))
        w = to_f32(0.5 * scale)
        scale = to_f32(0.5 / scale)
        return quat_normalize(
            (
                to_f32(to_f32(r21 - r12) * scale),
                to_f32(to_f32(r02 - r20) * scale),
                to_f32(to_f32(r10 - r01) * scale),
                w,
            )
        )
    if r00 > r11 and r00 > r22:
        scale = to_f32(math.sqrt(to_f32(1.0 + r00 - r11 - r22)))
        x = to_f32(0.5 * scale)
        scale = to_f32(0.5 / scale)
        return quat_normalize(
            (
                x,
                to_f32(to_f32(r01 + r10) * scale),
                to_f32(to_f32(r20 + r02) * scale),
                to_f32(to_f32(r21 - r12) * scale),
            )
        )
    if r11 > r22:
        scale = to_f32(math.sqrt(to_f32(1.0 + r11 - r00 - r22)))
        y = to_f32(0.5 * scale)
        scale = to_f32(0.5 / scale)
        return quat_normalize(
            (
                to_f32(to_f32(r01 + r10) * scale),
                y,
                to_f32(to_f32(r12 + r21) * scale),
                to_f32(to_f32(r02 - r20) * scale),
            )
        )
    scale = to_f32(math.sqrt(to_f32(1.0 + r22 - r00 - r11)))
    z = to_f32(0.5 * scale)
    scale = to_f32(0.5 / scale)
    return quat_normalize(
        (
            to_f32(to_f32(r02 + r20) * scale),
            to_f32(to_f32(r12 + r21) * scale),
            z,
            to_f32(to_f32(r10 - r01) * scale),
        )
    )


def dual_quat_from_matrix(m: Sequence[float]) -> Tuple[float, ...]:
    qx, qy, qz, qw = matrix_to_quat(m)
    tx, ty, tz = m[12], m[13], m[14]
    dx = 0.5 * (tx * qw + ty * qz - tz * qy)
    dy = 0.5 * (-tx * qz + ty * qw + tz * qx)
    dz = 0.5 * (tx * qy - ty * qx + tz * qw)
    dw = -0.5 * (tx * qx + ty * qy + tz * qz)
    return (qx, qy, qz, qw, dx, dy, dz, dw)


def build_inner_bdae(scene: Scene, offset_count_override: Optional[int] = None) -> bytes:
    legacy_layout = True
    string_table = collect_strings(scene)
    header_size = 60
    offset_table_offset = header_size
    offset_count = offset_count_override if offset_count_override is not None else (222 if scene.skins else 0)
    string_table_offset = offset_table_offset + offset_count * 4
    strings_blob, string_offsets = string_table.build(string_table_offset)

    b = BinaryBuilder()
    b.write(b"\0" * header_size)
    b.write(b"\0" * (offset_count * 4))
    b.write(strings_blob)
    b.align(4)
    removable_chunks: List[PendingRemovableChunk] = []
    removable_offset_patches: List[Tuple[int, int]] = []
    effect_layouts: List[Dict[str, object]] = []
    material_layouts: List[Dict[str, object]] = []
    mesh_layouts: List[Dict[str, object]] = []
    skin_layouts: List[Dict[str, object]] = []
    node_layouts: Dict[int, Dict[str, int]] = {}
    instance_layouts: Dict[int, List[Dict[str, int]]] = {}

    def add_removable(type_id: int, payload: bytearray, offset_patch: int) -> None:
        removable_offset_patches.append((offset_patch, len(removable_chunks)))
        removable_chunks.append(PendingRemovableChunk(type_id, aligned_payload(payload)))

    def add_pending_removable(chunk: PendingRemovableChunk, offset_patch: int) -> None:
        removable_offset_patches.append((offset_patch, len(removable_chunks)))
        removable_chunks.append(chunk)

    model_info_size = 188 if scene.skins else 180
    model_info_offset = b.tell()
    model_info_patch = b.write(b"\0" * model_info_size)

    legacy_config_offset = 0
    if legacy_layout:
        legacy_config_offset = b.tell()
        b.write(
            u32(1)
            + u32(0)
            + u32(1)
            + u32(legacy_config_offset + 16)
            + u32(0x7FFFFFFF)
            + u32(0x80000000)
            + u32(1)
            + u32(0)
            + u32(0)
            + u32(legacy_config_offset + 40)
            + u32(0)
        )

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
    material_info_by_name = {info.name: info for info in scene.materials}
    material_info_by_name.update({info.material_id: info for info in scene.materials})

    texture_metadata_offset = b.tell()
    if scene.images:
        for image in scene.images:
            b.write(
                u32(string_offsets[image.image_id])
                + u32(string_offsets[image.name])
                + u32(string_offsets[image.path])
                + u32(0)
                + u32(0)
            )
    else:
        for texture_name in textures:
            texture_offset = string_offsets.get(texture_name)
            if texture_offset is None:
                raise ValueError(f"internal error: missing string '{texture_name}'")
            b.write(u32(string_offsets["texture"]) + u32(string_offsets["texture"]) + u32(texture_offset) + u32(0) + u32(0))

    effect_metadata_offset = b.tell()
    for effect in scene.effects:
        effect_offset = b.tell()
        b.write(u32(string_offsets[effect.profile_id]) + u32(string_offsets[effect.profile_name]))
        placeholder_size = PROFILE_COMMON_EFFECT_SIZE - 8
        if placeholder_size < 0:
            raise ValueError("internal error: invalid ProfileCOMMON effect size")
        b.write(b"\0" * placeholder_size)
        effect_layouts.append(
            {
                "offset": effect_offset,
                "relocations": [effect_offset, effect_offset + 4]
                + [effect_offset + 8 + i * 4 for i in range(PROFILE_COMMON_EFFECT_RELOCATION_COUNT - 2)],
            }
        )

    material_metadata_offset = b.tell()
    material_records: List[Tuple[str, Optional[MaterialInfo]]] = []
    for material in materials:
        info = material_info_by_name.get(material)
        if info is None:
            mat_offset = string_offsets[material]
            material_layouts.append({"offset": b.tell(), "setparams": [], "setparam_patch": b.tell() + 20})
            material_records.append((material, None))
            b.write(u32(mat_offset) + u32(mat_offset) + u32(0) + u32(0) + u32(0) + u32(0) + u32(0) + u32(0) + u32(0))
            continue

        setparam_count = len(info.setparams)
        material_layout: Dict[str, object] = {
            "offset": b.tell(),
            "setparams": [],
            "setparam_patch": b.tell() + 20,
            "effect_file_offset": string_offsets[info.effect_file] if info.effect_file else 0,
        }
        material_layouts.append(material_layout)
        material_records.append((material, info))
        b.write(
            u32(string_offsets[info.material_id])
            + u32(string_offsets[info.name])
            + u32(string_offsets[info.effect_file] if info.effect_file else 0)
            + u32(string_offsets[info.effect_ref])
            + u32(setparam_count)
            + u32(0)
            + s32(-1 if info.effect_file else 0)
            + s32(-1)
            + u32(128)
        )

    for material_layout, (_, info) in zip(material_layouts, material_records):
        if info is None or not info.setparams:
            continue

        b.patch(int(material_layout["setparam_patch"]), u32(b.tell()))
        dummy_patches: List[int] = []
        value_patches: List[Tuple[int, SetParam]] = []
        for ref, sid, param_type, value in info.setparams:
            setparam_layout: Dict[str, int] = {"offset": b.tell(), "param_type": param_type}
            material_layout["setparams"].append(setparam_layout)  # type: ignore[index,union-attr]
            b.write(u32(string_offsets[ref]) + u32(string_offsets[""]) + u32(param_type) + u32(1))
            setparam_layout["dummy_patch"] = b.tell()
            dummy_patches.append(b.tell())
            b.write(u32(0))
            setparam_layout["value_patch"] = b.tell()
            value_patches.append((b.tell(), (ref, sid, param_type, value)))
            b.write(u32(0))

        for idx, (patch_offset, (_, sid, param_type, value)) in enumerate(zip(dummy_patches, info.setparams)):
            setparam_layout = material_layout["setparams"][idx]  # type: ignore[index]
            dummy_offset = b.tell()
            setparam_layout["dummy_child"] = dummy_offset
            b.patch(patch_offset, u32(dummy_offset))
            b.write(u32(1))
            patch_offset = value_patches.pop(0)[0]
            value_offset = b.tell()
            setparam_layout["value_child"] = value_offset
            b.patch(patch_offset, u32(value_offset))
            if param_type == 20:
                b.write(u32(1) + u32(string_offsets[sid]))
            else:
                sub_value_offset = b.tell() + 4
                b.write(u32(sub_value_offset) + u32(value))

    mesh_metadata_offset = b.tell()
    mesh_data_patches: List[int] = []
    for mesh in scene.meshes:
        mesh_id_offset = string_offsets[mesh.mesh_id or mesh.name]
        name_offset = string_offsets[mesh.name]
        b.write(u32(mesh_id_offset) + u32(name_offset) + u32(0))
        mesh_data_patches.append(b.tell())
        b.write(u32(0))

    for mesh, patch_offset in zip(scene.meshes, mesh_data_patches):
        mesh_data_offset = b.tell()
        b.patch(patch_offset, u32(mesh_data_offset))
        mesh_layout: Dict[str, object] = {"data": mesh_data_offset, "submeshes": []}
        mesh_layouts.append(mesh_layout)

        vertex_data_patch = mesh_data_offset + 80
        source_layout_offset = mesh_data_offset + 44
        bbox_min, bbox_max = bounds(mesh.vertices)
        has_color = any(vertex.color is not None for vertex in mesh.vertices)
        bytes_per_vertex = 36 if has_color else 32
        source_count = 4 if has_color else 3
        submesh_data_offset = mesh_data_offset + (88 + source_count * 16 if legacy_layout else 84)

        b.write(
            u32(1)
            + u32(len(mesh.vertices))
            + u32(source_layout_offset if legacy_layout else 40)
            + u32(len(mesh.submeshes))
            + u32(submesh_data_offset)
            + vec3(bbox_min)
            + vec3(bbox_max)
        )

        if legacy_layout:
            offsets_offset = mesh_data_offset + 88
            types_offset = offsets_offset + source_count * 4
            component_counts_offset = types_offset + source_count * 4
            scale_offsets_offset = component_counts_offset + source_count * 4
            b.write(
                u32(bytes_per_vertex)
                + u32(source_count)
                + u32(offsets_offset)
                + u32(source_count)
                + u32(types_offset)
                + u32(source_count)
                + u32(component_counts_offset)
                + u32(source_count)
                + u32(scale_offsets_offset)
                + u32(0)
                + u32(0)
            )
            if has_color:
                b.write(u32(0) + u32(12) + u32(32) + u32(24))
                b.write(u32(6) + u32(6) + u32(1) + u32(6))
                b.write(u32(3) + u32(3) + u32(4) + u32(2))
                b.write(u32(0) + u32(0) + u32(0) + u32(0))
            else:
                b.write(u32(0) + u32(12) + u32(24))
                b.write(u32(6) + u32(6) + u32(6))
                b.write(u32(3) + u32(3) + u32(2))
                b.write(u32(0) + u32(0) + u32(0))
        else:
            b.write(u32(32) + (u32(0) * 8) + u32(0))

        submesh_index_patches: List[int] = []
        for submesh in mesh.submeshes:
            submesh_offset = b.tell()
            mesh_layout["submeshes"].append(submesh_offset)  # type: ignore[index,union-attr]
            material_offset = string_offsets[submesh.material]
            indices = submesh.indices
            if legacy_layout:
                if has_color:
                    source_ids = bytes([0, 1, 255, 255, 3, 255, 255, 255, 2] + [255] * 9 + [0, 0])
                else:
                    source_ids = bytes([0, 1, 255, 255, 2] + [255] * 13 + [0, 0])
                b.write(u32(0))
                b.write(u32(material_offset))
                b.write(u32(len(indices) // 3))
                b.write(source_ids)
            else:
                b.write(u32(material_offset))
                b.write(u32(material_offset))
                b.write(s32(0) * 5)
            b.write(u32(min(indices)))
            b.write(u32(max(indices)))
            if not legacy_layout:
                b.write(u32(0))
            b.write(u32(len(indices)))
            submesh_index_patches.append(b.tell())
            b.write(u32(0))
            b.write(u32(0) + u32(0))

        add_removable(1, vertex_payload(mesh), vertex_data_patch)

        for submesh, index_patch in zip(mesh.submeshes, submesh_index_patches):
            add_removable(1, index_payload(submesh), index_patch)

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
        skin_layouts.append({"meta_patch": patch_offset, "data": skin_data_offset, "bone_count": bone_count})
        dual_quat_count = bone_count * 8
        influence_float_count = len(skin.influences) * (skin.max_influence + 1)

        bind_shape = [clean_f32(value) for value in skin.bind_shape_matrix[:16]]
        bind_shape += [0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0]

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
        for matrix in skin.inverse_bind_matrices:
            matrix_f32 = [to_f32(value) for value in matrix[:16]]
            b.write(b"".join(f32(value) for value in dual_quat_from_matrix(matrix_f32)))

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

        add_removable(2, influence_payload(skin), influence_data_patch)
        add_pending_removable(optimized_skin_payload(skin), unknown_data_patch)

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
        u32(string_offsets[scene.node_tree_id])
        + u32(string_offsets[scene.node_tree_name])
        + u32(len(root_nodes))
        + u32(0)
    )

    def write_node_block(nodes: Sequence[Node]) -> int:
        block_offset = b.tell()
        child_patches: List[Tuple[int, Sequence[Node]]] = []
        instance_patches: List[Tuple[int, Sequence[Instance]]] = []
        empty_offset = string_offsets[""]

        for node in nodes:
            node_offset = b.tell()
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
            instance_count = len(node.instances)
            node_layouts[id(node)] = {
                "offset": node_offset,
                "child_patch": child_offset_patch,
                "instance_patch": child_offset_patch + 8,
            }
            b.write(
                u32(0)
                + u32(instance_count)
                + u32(0)
                + u32(0)
                + u32(0)
            )

            if child_count:
                child_patches.append((child_offset_patch, node.children))
            if instance_count:
                instance_patches.append((child_offset_patch + 8, node.instances))

        for child_offset_patch, child_nodes in child_patches:
            child_offset = write_node_block(child_nodes)
            b.patch(child_offset_patch, u32(child_offset))

        for instance_offset_patch, instances in instance_patches:
            instance_offset = write_instances(instances)
            b.patch(instance_offset_patch, u32(instance_offset))

        return block_offset

    def write_url(url: str) -> None:
        if "#" in url and not url.startswith("#"):
            before, after = url.split("#", 1)
            b.write(u32(string_offsets[before]) + u32(string_offsets[f"#{after}"]))
        else:
            b.write(u32(0) + u32(string_offsets[url]))

    def write_instances(instances: Sequence[Instance]) -> int:
        offset = b.tell()
        layouts: List[Dict[str, int]] = []
        instance_layouts[id(instances)] = layouts
        for instance in instances:
            instance_offset = b.tell()
            b.write(u32(instance.type_id))
            instance_payload_offset = b.tell() + 4
            layout: Dict[str, int] = {"type": instance.type_id, "offset": instance_offset, "payload": instance_payload_offset}
            layouts.append(layout)
            b.write(u32(instance_payload_offset))
            if instance.type_id in (1, 4):
                write_url(instance.url)
                b.write(s32(-1))
            elif instance.type_id == 2:
                payload_start = b.tell()
                layout["payload"] = payload_start
                write_url(instance.url)
                b.write(u32(0))
                b.write(u32(1) + u32(payload_start + 32))
                b.write(u32(0) + u32(1) + u32(payload_start + 92))
                b.write(u32(0) + u32(string_offsets[instance.material_target]))
                b.write(u32(0) * 13)
                b.write(u32(string_offsets[instance.skeleton]))
            elif instance.type_id == 3:
                payload_start = b.tell()
                layout["payload"] = payload_start
                write_url(instance.url)
                b.write(u32(0))

                bind_material_offset = b.tell()
                material_url = instance.material_target
                material_index = -1
                if material_url.startswith("#"):
                    material_name = material_url[1:]
                    if material_name in materials:
                        material_index = materials.index(material_name)

                b.write(u32(1) + u32(bind_material_offset + 8))
                material_record_offset = b.tell()
                write_url(material_url)
                b.write(s32(material_index) + u32(0) + u32(0))
                for _ in range(4):
                    b.write(u32(0) + u32(0))
                b.write(u32(0) + u32(0))
                b.write(s32(0))
        return offset

    node_tree_data_offset = write_node_block(root_nodes)
    b.patch(node_tree_data_patch, u32(node_tree_data_offset))

    legacy_scene_offset = 0
    if legacy_layout:
        legacy_scene_offset = b.tell()
        scene_url = f"#{scene.node_tree_id}"
        scene_url_offset = string_offsets.get(scene_url, string_offsets.get(""))
        b.write(u32(6) + u32(legacy_scene_offset + 8) + u32(0) + u32(scene_url_offset))

    removable_offset = b.tell()
    include_removable_type = True
    removable_sizes = [(4 if include_removable_type else 0) + len(chunk.payload) for chunk in removable_chunks]
    metadata_size = 8 * len(removable_chunks)
    removable_starts: List[int] = []
    cursor = removable_offset + metadata_size
    for size in removable_sizes:
        removable_starts.append(cursor)
        cursor += size

    for patch_offset, chunk_index in removable_offset_patches:
        b.patch(patch_offset, u32(removable_starts[chunk_index]))

    for chunk, chunk_start in zip(removable_chunks, removable_starts):
        for patch_offset, target_offset in chunk.offset_patches:
            type_size = 4 if include_removable_type else 0
            chunk.payload[patch_offset : patch_offset + 4] = u32(chunk_start + type_size + target_offset)

    for size, start in zip(removable_sizes, removable_starts):
        b.write(u32(size) + u32(start))

    for chunk in removable_chunks:
        if include_removable_type:
            b.write(u32(chunk.type_id))
        b.write(bytes(chunk.payload))

    size_removable = b.tell() - removable_offset
    dynamic_size = 0
    file_size = b.tell()

    def build_offset_entries() -> List[int]:
        entries: List[int] = [24, 28, 32, 36, 40]
        entries.append(model_info_offset)
        if legacy_layout:
            entries += [model_info_offset + 48, legacy_config_offset + 12, legacy_config_offset + 36]

        entries.append(model_info_offset + 80)
        image_count = len(scene.images) if scene.images else len(textures)
        for i in range(image_count):
            image_offset = texture_metadata_offset + i * 20
            entries += [image_offset, image_offset + 4, image_offset + 8]

        if effect_layouts:
            entries.append(model_info_offset + 88)
            for effect_layout in effect_layouts:
                entries.extend(int(offset) for offset in effect_layout["relocations"])  # type: ignore[index]

        entries.append(model_info_offset + 96)
        for material_layout in material_layouts:
            material_offset = int(material_layout["offset"])
            entries += [material_offset, material_offset + 4]
            if int(material_layout.get("effect_file_offset", 0)):
                entries.append(material_offset + 8)
            entries += [material_offset + 12, material_offset + 20]
            for setparam_layout in material_layout["setparams"]:  # type: ignore[index]
                setparam_offset = int(setparam_layout["offset"])
                entries += [
                    setparam_offset,
                    setparam_offset + 4,
                    int(setparam_layout["dummy_patch"]),
                    int(setparam_layout["value_patch"]),
                ]
                if int(setparam_layout["param_type"]) == 11:
                    entries.append(int(setparam_layout["value_child"]))
                else:
                    entries.append(int(setparam_layout["value_child"]) + 4)

        entries.append(model_info_offset + 104)
        for mesh_index, mesh_layout in enumerate(mesh_layouts):
            mesh_meta_offset = mesh_metadata_offset + mesh_index * 16
            mesh_data_offset = int(mesh_layout["data"])
            entries += [mesh_meta_offset, mesh_meta_offset + 4, mesh_meta_offset + 12]
            entries += [mesh_data_offset + 8, mesh_data_offset + 52, mesh_data_offset + 60, mesh_data_offset + 68, mesh_data_offset + 76]
            entries.append(removable_offset + 4)
            entries.append(mesh_data_offset + 80)
            entries.append(mesh_data_offset + 16)
            for submesh_index, submesh_offset in enumerate(mesh_layout["submeshes"]):  # type: ignore[index]
                entries.append(int(submesh_offset) + 4)
                entries.append(removable_offset + 8 * (2 * mesh_index + submesh_index + 1) + 4)
                entries.append(int(submesh_offset) + 44)

        if skin_layouts:
            entries.append(model_info_offset + 112)
        mesh_removable_count = sum(1 + len(mesh.submeshes) for mesh in scene.meshes)
        for skin_index, skin_layout in enumerate(skin_layouts):
            skin_meta_offset = mesh_skin_metadata_offset + skin_index * 12
            skin_data_offset = int(skin_layout["data"])
            bone_count = int(skin_layout["bone_count"])
            entries += [skin_meta_offset + 4, skin_meta_offset + 8]
            entries += [skin_data_offset + 4, skin_data_offset + 12, skin_data_offset + 112, skin_data_offset + 120]
            bone_names_offset = skin_data_offset + 156 + bone_count * 16 * 4 + bone_count * 8 * 4
            for bone_index in range(bone_count):
                entries.append(bone_names_offset + bone_index * 4)
            influence_chunk_index = mesh_removable_count + skin_index * 2
            optimized_chunk_index = influence_chunk_index + 1
            entries.append(removable_offset + influence_chunk_index * 8 + 4)
            entries.append(skin_data_offset + 128)
            entries.append(removable_offset + optimized_chunk_index * 8 + 4)
            optimized_chunk_start = removable_starts[optimized_chunk_index]
            for bone_index in range(bone_count):
                entries.append(optimized_chunk_start + 8 + bone_index * 8)
            entries += [skin_data_offset + 136, skin_data_offset + 144]

        node_tree_info_offset = model_info_offset + (152 if scene.skins else 148)
        entries += [node_tree_info_offset, node_tree_metadata_offset, node_tree_metadata_offset + 4, node_tree_metadata_offset + 12]

        def add_instance_entries(instances: Sequence[Instance]) -> None:
            for layout in instance_layouts.get(id(instances), []):
                instance_offset = layout["offset"]
                payload_offset = layout["payload"]
                type_id = layout["type"]
                entries.append(instance_offset + 4)
                if type_id in (1, 4, 6):
                    entries.append(payload_offset + 4)
                elif type_id == 2:
                    entries.extend([payload_offset + 4, payload_offset + 16, payload_offset + 36, payload_offset + 28, payload_offset + 92])
                elif type_id == 3:
                    entries.extend([payload_offset + 4, payload_offset + 16, payload_offset + 24])

        def add_node_entries(nodes: Sequence[Node]) -> None:
            for node in nodes:
                layout = node_layouts[id(node)]
                node_offset = layout["offset"]
                entries.extend([node_offset, node_offset + 4, node_offset + 8])
                if node.children:
                    entries.append(layout["child_patch"])
                    add_node_entries(node.children)
                if node.instances:
                    entries.append(layout["instance_patch"])
                    add_instance_entries(node.instances)

        add_node_entries(root_nodes)
        if legacy_layout:
            entries.append(model_info_offset + (184 if scene.skins else 172))
            entries += [legacy_scene_offset + 4, legacy_scene_offset + 12]
        return entries

    offset_entries = build_offset_entries() if legacy_layout else []
    if legacy_layout:
        if len(offset_entries) != offset_count:
            if offset_count_override is None:
                return build_inner_bdae(scene, len(offset_entries))
            raise ValueError(f"internal error: generated {len(offset_entries)} relocation offsets, expected {offset_count}")
        b.patch(offset_table_offset, b"".join(u32(entry) for entry in offset_entries))

    model_info = bytearray()
    model_info += u32(string_offsets[BDAE_VERSION_STRING])
    if legacy_layout:
        model_info += (
            u32(0)
            + u32(0)
            + u32(1)
            + u32(0)
            + u32(15 if scene.skins else 0)
            + u32(0)
            + u32(0x7FFFFFFF)
            + u32(0x80000000)
            + u32(0)
            + u32(0)
            + u32(0)
            + u32(legacy_config_offset + 8)
            + u32(0)
            + u32(0)
            + u32(0)
            + u32(0)
            + u32(0)
            + u32(0)
        )
    else:
        model_info += u32(0) * 6
        model_info += s32(0)
        model_info += s32(0)
        model_info += u32(0)
        model_info += u32(0)
        model_info += u32(0) * 8
    model_info += u32(len(textures))
    model_info += u32(texture_metadata_offset)
    model_info += u32(len(scene.effects))
    model_info += u32(effect_metadata_offset if scene.effects else 0)
    model_info += u32(len(materials))
    model_info += u32(material_metadata_offset)
    model_info += u32(len(scene.meshes))
    model_info += u32(mesh_metadata_offset)
    model_info += u32(len(scene.skins))
    model_info += u32(mesh_skin_metadata_offset if scene.skins else 0)
    model_info += (u32(0) + u32(0)) * 4
    model_info += u32(1)
    model_info += u32(node_tree_metadata_offset)
    if legacy_layout:
        model_info += (u32(0) + u32(0)) * (3 if scene.skins else 2)
        model_info += u32(1) + u32(legacy_scene_offset)
    else:
        model_info += (u32(0) + u32(0)) * 4
    assert len(model_info) == (188 if scene.skins else 180)
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
    header += u32(legacy_config_offset if legacy_layout else 0)
    header += u32(removable_offset)
    header += u32(size_removable)
    header += u32(len(removable_chunks))
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


def find_bdae_archive(folder: Path) -> Path:
    archives = sorted(path for path in folder.iterdir() if path.is_file() and path.suffix.lower() == ".bdae")
    if not archives:
        raise FileNotFoundError(f"no .bdae archive found in folder: {folder}")
    return archives[0]


def extract_source_dae(archive_path: Path, output_path: Path) -> None:
    with zipfile.ZipFile(archive_path) as archive:
        source_name = next((name for name in archive.namelist() if name.lower() == "source.dae"), None)
        if source_name is None:
            raise FileNotFoundError(f"source.dae not found inside archive: {archive_path}")
        output_path.write_bytes(archive.read(source_name))


def prepare_input_dae(input_path: Path, temp_dir: Path) -> Tuple[Path, Path]:
    if input_path.is_dir():
        archive_path = find_bdae_archive(input_path)
        dae_path = temp_dir / f"{archive_path.stem}_source.dae"
        extract_source_dae(archive_path, dae_path)
        return dae_path, archive_path

    if input_path.suffix.lower() == ".bdae":
        dae_path = temp_dir / f"{input_path.stem}_source.dae"
        extract_source_dae(input_path, dae_path)
        return dae_path, input_path

    if input_path.suffix.lower() == ".dae":
        return input_path, input_path

    raise ValueError("input must be a .dae file, a .bdae archive, or a folder containing a .bdae archive")


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Convert COLLADA geometry to 32-bit OaC BDAE.")
    parser.add_argument("input", type=Path, help="input .dae, .bdae archive with source.dae, or folder containing a .bdae archive")
    parser.add_argument("-o", "--output", type=Path, help="output .bdae file")
    parser.add_argument("--inner", action="store_true", help="write raw inner BDAE instead of outer ZIP package")
    parser.add_argument("--no-source", action="store_true", help="do not include source.dae in the outer archive")
    parser.add_argument("--import-temp", action="store_true", help="write the viewer import output to data/tmp")
    args = parser.parse_args(argv)

    if args.import_temp and args.output is not None:
        parser.error("--import-temp cannot be used with --output")

    input_path = args.input
    if not input_path.exists():
        parser.error(f"input not found: {input_path}")

    with tempfile.TemporaryDirectory(prefix="dae2bdae_") as temp_name:
        try:
            dae_path, source_origin = prepare_input_dae(input_path, Path(temp_name))
        except (FileNotFoundError, ValueError, zipfile.BadZipFile) as exc:
            parser.error(str(exc))

        if args.import_temp:
            output_path = Path("data/tmp/imported.bdae")
            output_path.parent.mkdir(parents=True, exist_ok=True)
        else:
            output_path = args.output

        if output_path is None:
            output_path = source_origin.with_name(default_output_stem(source_origin)).with_suffix(
                ".inner.bdae" if args.inner else ".bdae"
            )

        scene = parse_dae(dae_path)
        inner = build_inner_bdae(scene)

        if args.inner:
            output_path.write_bytes(inner)
        else:
            write_outer_bdae(inner, output_path, None if args.no_source else dae_path)

        vertex_count = sum(len(mesh.vertices) for mesh in scene.meshes)
        face_count = sum(len(submesh.indices) // 3 for mesh in scene.meshes for submesh in mesh.submeshes)
        print(
            f"wrote {output_path} ({len(scene.meshes)} mesh(es), "
            f"{vertex_count} vertices, {face_count} triangles)"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
