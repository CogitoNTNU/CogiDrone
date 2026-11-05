"""Sanntids objektdeteksjon og dybdeestimering for 6 S&R menneske-klasser med YOLO."""

import sys
from pathlib import Path

import cv2
import numpy as np
import torch
from PIL import Image
from transformers import pipeline as hf_pipeline  # Renamed import to avoid any namespace overlap
from ultralytics import YOLO

# Importer kamerafunksjoner og sjekk fra din oppdaterte camera.py
from camera import open_webcam, close_webcam, is_realsense_available
import pyrealsense2 as rs

# Gjør targeter/-mappen importerbar (ligger ved siden av inference/)
sys.path.append(str(Path(__file__).resolve().parent.parent))
from targeter.target_closest import ClosestTargeter, Detection, is_too_close

# ==================== KONFIGURASJON (Endre her) ====================
MODEL_WEIGHTS = "models/best.pt" #"models/yolo11n.pt" 
HEAD_MODEL_WEIGHTS = "models/medium.pt"
CAMERA_INDEX = 1
CONF_THRESHOLD = 0.20

# Dine 6 egendefinerte S&R-klasser (må matche 100% med data.yaml rekkefølge)
SR_CLASS_NAMES = [
    'running',
    'walking',
    'laying_down',
    'human_not_defined',
    'seated',
    'stands'
]
# ===================================================================

AVERAGE_HUMAN_HEIGHT_M = 1.70
AVERAGE_HEAD_HEIGHT_M = 0.23      # Gjennomsnittlig hodehøyde (~23cm)

def offset_from_center(x1, y1, x2, y2, frame_w, frame_h):
    """Returnerer (dx, dy) i piksler fra skjermens midte til midten av boksen."""
    cx = (x1 + x2) / 2
    cy = (y1 + y2) / 2
    dx = cx - frame_w / 2
    dy = cy - frame_h / 2
    return dx, dy


def main() -> None:
    # Initialiser modeller
    model = YOLO(MODEL_WEIGHTS)
    head_model = YOLO(HEAD_MODEL_WEIGHTS)
    
    # Sjekk kameratype og sett opp strømming
    using_realsense = is_realsense_available()
    
    cap = None
    rs_pipeline = None  # Renamed from pipeline to fix the crash
    depth_pipe = None
    
    if using_realsense:
        print("RealSense-kamera funnet! Starter maskinvare-pipeline...")
        rs_pipeline = rs.pipeline()
        config = rs.config()
        config.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
        config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)
        rs_pipeline.start(config)
    else:
        print("Ingen RealSense funnet. Bruker standard webcam + Depth Anything V2...")
        cap = open_webcam(width=640, height=480, camera_index=CAMERA_INDEX)
        
        print("Laster inn Depth Anything V2...")
        device = "cuda" if torch.cuda.is_available() else "cpu"
        torch_dtype = torch.float16 if device == "cuda" else torch.float32
        
        # Uses the renamed safe import token
        depth_pipe = hf_pipeline(
            task="depth-estimation",
            model="depth-anything/Depth-Anything-V2-Small-hf",
            torch_dtype=torch_dtype,
            device=0 if device == "cuda" else -1
        )
    
    print("Modeller og kamera klare!")

    targeter = ClosestTargeter()
    frame_count = 0
    cached_depth_map = None
    depth_interval = 2  # Kjør dybdemodell per x frame (kun brukt for webcam fallback)

    try:
        while True:
            # Hent råbilder basert på aktiv kameratype
            if using_realsense:
                frames = rs_pipeline.wait_for_frames()
                depth_frame = frames.get_depth_frame()
                color_frame = frames.get_color_frame()

                if not depth_frame or not color_frame:
                    continue

                # Konverter til numpy arrays
                img = np.asanyarray(color_frame.get_data())
                
                # Generer fargelagt dybdekart direkte fra maskinvarens rådata
                raw_depth = np.asanyarray(depth_frame.get_data())
                depth_colormap = cv2.applyColorMap(
                    cv2.convertScaleAbs(raw_depth, alpha=0.03), cv2.COLORMAP_JET
                )
            else:
                success, img = cap.read()
                if not success:
                    continue
                
                # Generer dybdekart virtuelt via AI
                frame_count += 1
                h, w, _ = img.shape
                if frame_count % depth_interval == 1 or cached_depth_map is None:
                    pil_img = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
                    depth_output = depth_pipe(pil_img)
                    raw_depth = np.array(depth_output["depth"])
                    cached_depth_map = cv2.resize(raw_depth, (w, h))
                
                depth_colormap = cv2.applyColorMap(
                    cv2.convertScaleAbs(cached_depth_map, alpha=0.03), cv2.COLORMAP_JET
                )

            h, w, _ = img.shape
            focal_length = w * 0.8  # Skalert brennvidde basert på bredde

            # Primær YOLO-deteksjon kjøres utelukkende på det rene RGB-bildet
            results = model(
                img, imgsz=640, conf=CONF_THRESHOLD, stream=True, verbose=False
            )

            detections = []  
            for r in results:
                for box in r.boxes:
                    x1, y1, x2, y2 = map(int, box.xyxy[0])
                    cls = int(box.cls[0])
                    confidence = float(box.conf[0])

                    if cls < 0 or cls >= len(SR_CLASS_NAMES):
                        continue  

                    cls_name = SR_CLASS_NAMES[cls]
                    bbox_h = max(y2 - y1, 1)
                    geom_dist = (AVERAGE_HUMAN_HEIGHT_M * focal_length) / bbox_h

                    head_dist = None
                    crop_x1, crop_y1 = max(0, x1), max(0, y1)
                    crop_x2, crop_y2 = min(w, x2), min(h, y2)
                    person_crop = img[crop_y1:crop_y2, crop_x1:crop_x2]

                    if person_crop.size > 0:
                        head_res = head_model(person_crop, imgsz=640, verbose=False)
                        if head_res and len(head_res[0].boxes) > 0:
                            best_head_idx = int(torch.argmax(head_res[0].boxes.conf).item())
                            h_box = head_res[0].boxes[best_head_idx]
                            
                            hx1, hy1, hx2, hy2 = map(int, h_box.xyxy[0].cpu().numpy())
                            global_hx1 = crop_x1 + hx1
                            global_hy1 = crop_y1 + hy1
                            global_hx2 = crop_x1 + hx2
                            global_hy2 = crop_y1 + hy2

                            head_h = max(hy2 - hy1, 1)
                            head_dist = (AVERAGE_HEAD_HEIGHT_M * focal_length) / head_h

                            # Tegn hodeboks i RØD på RGB-bildet
                            cv2.rectangle(img, (global_hx1, global_hy1), (global_hx2, global_hy2), (0, 0, 255), 2)

                    distance = head_dist if head_dist is not None else geom_dist
                    detections.append(Detection((x1, y1, x2, y2), confidence, distance))

                    # Tegn hovedboks i MAGENTA på RGB-bildet
                    cv2.rectangle(img, (x1, y1), (x2, y2), (255, 0, 255), 3)

                    dx, dy = offset_from_center(x1, y1, x2, y2, w, h)
                    box_cx, box_cy = (x1 + x2) // 2, (y1 + y2) // 2
                    cv2.line(img, (w // 2, h // 2), (box_cx, box_cy), (0, 255, 0), 2)

                    cv2.putText(
                        img, f"dx={dx:.0f} dy={dy:.0f}", (x1, min(y2 + 25, h - 10)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2
                    )

                    if head_dist is not None:
                        label = f"{cls_name} | Body: ~{geom_dist:.1f}m | Head: ~{head_dist:.1f}m"
                    else:
                        label = f"{cls_name} | Body: ~{geom_dist:.1f}m"

                    cv2.putText(
                        img, label, (x1, max(y1 - 10, 20)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 0), 2
                    )

            # Target markering på RGB-bilde
            target = targeter.update(detections)
            if target is not None:
                tx1, ty1, tx2, ty2 = target.bbox
                too_close = is_too_close(target)
                color = (0, 0, 255) if too_close else (0, 255, 255)
                text = "TARGET - FOR NAER" if too_close else f"TARGET {target.distance:.1f}m"
                cv2.rectangle(img, (tx1, ty1), (tx2, ty2), color, 5)
                cv2.putText(
                    img, text, (tx1, max(ty1 - 35, 20)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2
                )

            # Sikte-kryss i midten av RGB-bilde
            cv2.drawMarker(img, (w // 2, h // 2), (0, 255, 255), cv2.MARKER_CROSS, 30, 2)
            
            # --- SEPARATE STREAMING VINDUER ---
            # Vindu 1: Rent RGB Bilde med YOLO-overlays og målinger
            cv2.imshow("Stream | RGB Tracking & Recognition", img)
            
            # Vindu 2: Dybdebilde (Avstandskart) uten forstyrrende tekst
            cv2.imshow("Stream | Distance Map (Depth)", depth_colormap)

            if cv2.waitKey(1) == ord("q"):
                break
    finally:
        # Rydd opp uansett hvilken kilde som ble startet
        if using_realsense and rs_pipeline is not None:
            rs_pipeline.stop()
            cv2.destroyAllWindows()
        elif cap is not None:
            close_webcam(cap)

if __name__ == "__main__":
    main()
