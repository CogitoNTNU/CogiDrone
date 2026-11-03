"""Minimal binary glTF 2.0 writer (numpy only).

Coordinates are written as given. Gazebo (gz-common assimp loader) uses glTF positions as-is,
so our meshes are stored Z-up in metres, matching SDF.
"""
import json
import struct

import numpy as np


def _pad4(b, fill=b"\x00"):
    return b + fill * ((4 - len(b) % 4) % 4)


def write_glb(path, prims, materials):
    """prims: list of dicts {pos (N,3) f32, nrm (N,3) f32, uv (N,2) f32, idx (M,3) u32, material: name}.
    materials: dict name -> {"image": uri or None, "color": (r,g,b,a), "alpha": "OPAQUE"|"MASK",
    "double_sided": bool}.  Empty primitives are skipped."""
    prims = [p for p in prims if len(p["idx"])]
    bin_chunks = []
    offset = 0
    buffer_views, accessors, meshes_prims = [], [], []

    def add_view(arr, target):
        nonlocal offset
        b = _pad4(np.ascontiguousarray(arr).tobytes())
        buffer_views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(np.ascontiguousarray(arr).tobytes()),
                             "target": target})
        bin_chunks.append(b)
        offset += len(b)
        return len(buffer_views) - 1

    mat_names = list(dict.fromkeys(p["material"] for p in prims))
    images, textures, gl_mats = [], [], []
    for name in mat_names:
        m = materials[name]
        pbr = {"baseColorFactor": list(m.get("color", (1, 1, 1, 1))), "metallicFactor": 0.0,
               "roughnessFactor": m.get("roughness", 0.9)}
        if m.get("image"):
            images.append({"uri": m["image"]})
            textures.append({"source": len(images) - 1, "sampler": 0})
            pbr["baseColorTexture"] = {"index": len(textures) - 1}
        gm = {"name": name, "pbrMetallicRoughness": pbr, "doubleSided": bool(m.get("double_sided", False)),
              "alphaMode": m.get("alpha", "OPAQUE")}
        if gm["alphaMode"] == "MASK":
            gm["alphaCutoff"] = 0.5
        gl_mats.append(gm)

    for p in prims:
        pos = np.asarray(p["pos"], np.float32)
        nrm = np.asarray(p["nrm"], np.float32)
        uv = np.asarray(p["uv"], np.float32)
        idx = np.asarray(p["idx"], np.uint32).reshape(-1)
        n = len(pos)
        a_pos = len(accessors)
        accessors.append({"bufferView": add_view(pos, 34962), "componentType": 5126, "count": n, "type": "VEC3",
                          "min": pos.min(0).tolist(), "max": pos.max(0).tolist()})
        accessors.append({"bufferView": add_view(nrm, 34962), "componentType": 5126, "count": n, "type": "VEC3"})
        accessors.append({"bufferView": add_view(uv, 34962), "componentType": 5126, "count": n, "type": "VEC2"})
        accessors.append({"bufferView": add_view(idx, 34963), "componentType": 5125, "count": len(idx),
                          "type": "SCALAR"})
        meshes_prims.append({"attributes": {"POSITION": a_pos, "NORMAL": a_pos + 1, "TEXCOORD_0": a_pos + 2},
                             "indices": a_pos + 3, "material": mat_names.index(p["material"]), "mode": 4})

    gltf = {"asset": {"version": "2.0", "generator": "cogidrone sim/tools/trees"},
            "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": "tree"}],
            "meshes": [{"name": "tree", "primitives": meshes_prims}],
            "materials": gl_mats, "buffers": [{"byteLength": offset}],
            "bufferViews": buffer_views, "accessors": accessors}
    if images:
        gltf["images"] = images
        gltf["textures"] = textures
        gltf["samplers"] = [{"magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33071}]
    js = _pad4(json.dumps(gltf, separators=(",", ":")).encode(), b" ")
    binb = b"".join(bin_chunks)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(binb)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A)); f.write(js)
        f.write(struct.pack("<II", len(binb), 0x004E4942)); f.write(binb)
