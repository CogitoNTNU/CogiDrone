"""Minimal numpy-only glTF-binary (.glb) reader/writer for triangle meshes.

Only what the props pipeline needs: one mesh, one primitive, attributes
POSITION / NORMAL / TEXCOORD_0 (float32) and uint32 indices. Coordinates are
written verbatim (we store Z-up; Gazebo uses glTF vertex data as-is).
UVs use the glTF convention (origin = top-left of the image).
"""
import json
import struct

import numpy as np

_GLB_MAGIC = 0x46546C67
_JSON = 0x4E4F534A
_BIN = 0x004E4942


def _pad4(b, fill=b"\x00"):
    return b + fill * ((4 - len(b) % 4) % 4)


def write_glb(path, positions, normals, uvs, indices, name="mesh", extras=None):
    pos = np.ascontiguousarray(positions, np.float32)
    nrm = np.ascontiguousarray(normals, np.float32)
    uv = np.ascontiguousarray(uvs, np.float32)
    idx = np.ascontiguousarray(indices, np.uint32).reshape(-1)
    blobs, views, accs = [], [], []
    off = 0

    def add(arr, target, comp, typ, count, minmax=False):
        nonlocal off
        b = arr.tobytes()
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(b), "target": target})
        acc = {"bufferView": len(views) - 1, "componentType": comp, "count": int(count), "type": typ}
        if minmax:
            acc["min"] = [float(v) for v in arr.min(0)]
            acc["max"] = [float(v) for v in arr.max(0)]
        accs.append(acc)
        b = _pad4(b)
        blobs.append(b)
        off += len(b)
        return len(accs) - 1

    a_pos = add(pos, 34962, 5126, "VEC3", len(pos), minmax=True)
    a_nrm = add(nrm, 34962, 5126, "VEC3", len(nrm))
    a_uv = add(uv, 34962, 5126, "VEC2", len(uv))
    a_idx = add(idx, 34963, 5125, "SCALAR", len(idx))
    binb = b"".join(blobs)
    gl = {
        "asset": {"version": "2.0", "generator": "cogidrone props glb.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": name}],
        "materials": [{"name": name + "_mat", "pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": 0.9}}],
        "meshes": [{"name": name, "primitives": [{
            "attributes": {"POSITION": a_pos, "NORMAL": a_nrm, "TEXCOORD_0": a_uv},
            "indices": a_idx, "material": 0, "mode": 4}]}],
        "buffers": [{"byteLength": len(binb)}],
        "bufferViews": views,
        "accessors": accs,
    }
    if extras:
        gl["extras"] = extras
    js = _pad4(json.dumps(gl, separators=(",", ":")).encode(), b" ")
    total = 12 + 8 + len(js) + 8 + len(binb)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", _GLB_MAGIC, 2, total))
        f.write(struct.pack("<II", len(js), _JSON))
        f.write(js)
        f.write(struct.pack("<II", len(binb), _BIN))
        f.write(binb)


def read_glb(path):
    """Returns dict(positions, normals, uvs, indices, extras) for files written by write_glb."""
    with open(path, "rb") as f:
        data = f.read()
    magic, _, _ = struct.unpack_from("<III", data, 0)
    assert magic == _GLB_MAGIC, path
    jl, _ = struct.unpack_from("<II", data, 12)
    gl = json.loads(data[20:20 + jl])
    bo = 20 + jl
    bl, _ = struct.unpack_from("<II", data, bo)
    binb = memoryview(data)[bo + 8:bo + 8 + bl]
    comp = {5126: np.float32, 5125: np.uint32, 5123: np.uint16}
    ncomp = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}

    def acc(i):
        a = gl["accessors"][i]
        v = gl["bufferViews"][a["bufferView"]]
        dt = comp[a["componentType"]]
        n = a["count"] * ncomp[a["type"]]
        arr = np.frombuffer(binb, dt, n, v.get("byteOffset", 0) + a.get("byteOffset", 0))
        return arr.reshape(a["count"], -1) if ncomp[a["type"]] > 1 else arr

    p = gl["meshes"][0]["primitives"][0]
    at = p["attributes"]
    return {
        "positions": acc(at["POSITION"]).astype(np.float32),
        "normals": acc(at["NORMAL"]).astype(np.float32),
        "uvs": acc(at["TEXCOORD_0"]).astype(np.float32),
        "indices": acc(p["indices"]).astype(np.uint32).reshape(-1, 3),
        "extras": gl.get("extras", {}),
    }
