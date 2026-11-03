"""Convert STEP to GLB with OpenCASCADE's tessellator: one mesh per solid, STEP names and colours kept.

usage: python step2glb_ocp.py in.step out.glb [linear_deflection_mm]
"""
import sys, json, numpy as np, trimesh
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TDocStd import TDocStd_Document
from OCP.TCollection import TCollection_ExtendedString
from OCP.XCAFDoc import XCAFDoc_DocumentTool, XCAFDoc_ColorType
from OCP.collections import Sequence_TDF_Label as TDF_LabelSequence
from OCP.TDF import TDF_Label
from OCP.TDataStd import TDataStd_Name
from OCP.Quantity import Quantity_Color
from OCP.TopLoc import TopLoc_Location
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.TopExp import TopExp_Explorer
from OCP.TopAbs import TopAbs_SOLID, TopAbs_FACE, TopAbs_REVERSED
from OCP.TopoDS import TopoDS
from OCP.BRep import BRep_Tool

src, dst = sys.argv[1], sys.argv[2]
defl = float(sys.argv[3]) if len(sys.argv) > 3 else 0.3

doc = TDocStd_Document(TCollection_ExtendedString("doc"))
r = STEPCAFControl_Reader()
r.SetNameMode(True)
r.SetColorMode(True)
assert r.ReadFile(src) == 1, "STEP read failed"
r.Transfer(doc)
st = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
ct = XCAFDoc_DocumentTool.ColorTool_s(doc.Main())


def name_of(lab):
    a = TDataStd_Name()
    return a.Get().ToExtString() if lab.FindAttribute(TDataStd_Name.GetID_s(), a) else ""


def color_of(lab, shape):
    c = Quantity_Color()
    for t in (XCAFDoc_ColorType.XCAFDoc_ColorSurf, XCAFDoc_ColorType.XCAFDoc_ColorGen):
        try:
            ok = ct.GetColor(shape, t, c)
        except Exception:
            ok = False
        if ok:
            return (c.Red(), c.Green(), c.Blue())
    return None


def mesh_shape(shape):
    BRepMesh_IncrementalMesh(shape, defl, False, 0.5, True)
    V, F, off = [], [], 0
    ex = TopExp_Explorer(shape, TopAbs_FACE)
    while ex.More():
        face = TopoDS.Face(ex.Current())
        loc = TopLoc_Location()
        tri = BRep_Tool.Triangulation_s(face, loc)
        if tri is not None:
            tr = loc.Transformation()
            pts = []
            for i in range(1, tri.NbNodes() + 1):
                p = tri.Node(i).Transformed(tr)
                pts.append((p.X(), p.Y(), p.Z()))
            rev = face.Orientation() == TopAbs_REVERSED
            for i in range(1, tri.NbTriangles() + 1):
                a, b, c = tri.Triangle(i).Get()
                F.append((a - 1 + off, c - 1 + off, b - 1 + off) if rev else (a - 1 + off, b - 1 + off, c - 1 + off))
            V.extend(pts)
            off += len(pts)
        ex.Next()
    if not F:
        return None
    return trimesh.Trimesh(np.array(V), np.array(F), process=True)


scene, info = trimesh.Scene(), []


def walk(lab, loc, pname, pcol):
    shape = st.GetShape_s(lab)
    nm = name_of(lab) or pname
    col = color_of(lab, shape) or pcol
    if st.IsAssembly_s(lab):
        comps = TDF_LabelSequence()
        st.GetComponents_s(lab, comps)
        for i in range(1, comps.Length() + 1):
            c = comps.Value(i)
            ref = TDF_Label()
            cloc = st.GetLocation_s(c)
            if st.GetReferredShape_s(c, ref):
                walk(ref, loc.Multiplied(cloc), name_of(c) or nm, color_of(c, st.GetShape_s(c)) or col)
        return
    shape = shape.Moved(loc)
    ex, k = TopExp_Explorer(shape, TopAbs_SOLID), 0
    solids = []
    while ex.More():
        solids.append(ex.Current())
        ex.Next()
    for s in solids or [shape]:
        m = mesh_shape(s)
        if m is None:
            continue
        if col:
            m.visual.face_colors = [int(col[0] * 255), int(col[1] * 255), int(col[2] * 255), 255]
        n = f"{nm or 'part'}_{len(info)}"[:63]
        scene.add_geometry(m, node_name=n, geom_name=n)
        lo, hi = m.bounds
        info.append({"name": n, "tris": len(m.faces), "c": [round(x, 1) for x in (lo + hi) / 2], "size": [round(x, 1) for x in (hi - lo)]})


roots = TDF_LabelSequence()
st.GetFreeShapes(roots)
for i in range(1, roots.Length() + 1):
    walk(roots.Value(i), TopLoc_Location(), "", None)
scene.export(dst)
lo, hi = scene.bounds
print(json.dumps({"parts": len(info), "bounds_mm": [lo.round(1).tolist(), hi.round(1).tolist()],
                  "tris": sum(i["tris"] for i in info), "parts_info": info}))
