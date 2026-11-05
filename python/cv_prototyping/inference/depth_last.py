"""Sanntids objektdeteksjon og avstandsestimering for 6 S&R-klasser med YOLO."""

import sys
from pathlib import Path

import cv2
import numpy as np
import torch
from ultralytics import YOLO

# Importer kamerafunksjoner og sjekk fra din oppdaterte camera.py
from camera import open_webcam, close_webcam, is_realsense_available
import pyrealsense2 as rs

# Gjør targeter/-mappen importerbar (ligger ved siden av inference/)
sys.path.append(str(Path(__file__).resolve().parent.parent))
from targeter.target_closest import ClosestTargeter, Detection

# ==================== KONFIGURASJON ====================
MODEL_WEIGHTS = "models/yolo26n.pt" #"models/best.pt" 
HEAD_MODEL_WEIGHTS = "models/medium.pt"
CAMERA_INDEX = 1
CONF_THRESHOLD = 0.20

# Kontrollerer om vinduer og GUI-tegninger skal vises (Sett til True for debugging)
ENABLE_VISUALIZATION = True

SR_CLASS_NAMES = [
    'running',
    'walking',
    'laying_down',
    'human_not_defined',
    'seated',
    'stands'
]
# =======================================================

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
    # Initialiser YOLO-modeller
    model = YOLO(MODEL_WEIGHTS)
    head_model = YOLO(HEAD_MODEL_WEIGHTS)
    
    # Sjekk kameratype og sett opp maskinvarestrøm
    using_realsense = is_realsense_available()
    
    cap = None
    rs_pipeline = None  
    
    if using_realsense:
        print("RealSense-kamera funnet! Starter maskinvare-pipeline...")
        rs_pipeline = rs.pipeline()
        config = rs.config()
        config.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
        config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)
        rs_pipeline.start(config)
    else:
        print("Ingen RealSense funnet. Bruker standard webcam...")
        cap = open_webcam(width=640, height=480, camera_index=CAMERA_INDEX)
        
    print("Modeller og kamera klare!")
    targeter = ClosestTargeter()

    try:
        while True:
            # Hent råbilder basert på aktiv kameratype
            if using_realsense:
                frames = rs_pipeline.wait_for_frames()
                color_frame = frames.get_color_frame()
                if not color_frame:
                    continue
                img = np.asanyarray(color_frame.get_data())
            else:
                success, img = cap.read()
                if not success:
                    continue

            h, w, _ = img.shape
            focal_length = w * 0.8  # Skalert brennvidde basert på bredde
            
            # Lag en ren kopi til visuelle overlays hvis det er aktivert
            display_img = img.copy() if ENABLE_VISUALIZATION else None

            # Primær YOLO-deteksjon kjøres utelukkende på det rene RGB-bildet
            results = model(img, imgsz=640, conf=CONF_THRESHOLD, stream=True, verbose=False)

            detections = []  
            detected_targets_log = []

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

                    # Beregn posisjonsforskyvning fra bildesentrum
                    dx, dy = offset_from_center(x1, y1, x2, y2, w, h)

                    # Hode-lokalisering for mer presis distanseestimering
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
                            head_h = max(hy2 - hy1, 1)
                            head_dist = (AVERAGE_HEAD_HEIGHT_M * focal_length) / head_h

                            if ENABLE_VISUALIZATION:
                                global_hx1 = crop_x1 + hx1
                                global_hy1 = crop_y1 + hy1
                                global_hx2 = crop_x1 + hx2
                                global_hy2 = crop_y1 + hy2
                                cv2.rectangle(display_img, (global_hx1, global_hy1), (global_hx2, global_hy2), (0, 0, 255), 2)

                    distance = head_dist if head_dist is not None else geom_dist
                    detections.append(Detection((x1, y1, x2, y2), confidence, distance))
                    
                    # Lagre rå-måledata for denne deteksjonen
                    detected_targets_log.append({
                        "class": cls_name,
                        "distance_m": round(distance, 2),
                        "offset_dx": round(dx, 1),
                        "offset_dy": round(dy, 1)
                    })

                    # Håndter debug-tegninger hvis skrudd på
                    if ENABLE_VISUALIZATION:
                        cv2.rectangle(display_img, (x1, y1), (x2, y2), (255, 0, 255), 3)
                        box_cx, box_cy = (x1 + x2) // 2, (y1 + y2) // 2
                        cv2.line(display_img, (w // 2, h // 2), (box_cx, box_cy), (0, 255, 0), 2)
                        cv2.putText(display_img, f"dx={dx:.0f} dy={dy:.0f}", (x1, min(y2 + 25, h - 10)),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
                        label = f"{cls_name} | Distance: ~{distance:.1f}m"
                        cv2.putText(display_img, label, (x1, max(y1 - 10, 20)),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 0), 2)

            # Oppdater målutvelger
            target = targeter.update(detections)
            
            # Skriv ut data-payloads for alle lokaliserte mål til konsoll/system logg
            if detected_targets_log:
                print(f"Lokaliserte objekter denne framen: {detected_targets_log}")
                if target is not None:
                    tx1, ty1, tx2, ty2 = target.bbox
                    tdx, tdy = offset_from_center(tx1, ty1, tx2, ty2, w, h)
                    print(f"--> AKTIVT TARGET VALGT: Klasse={cls_name} | Dist={target.distance:.2f}m | dx={tdx:.1f} dy={tdy:.1f}")
                    
                    if ENABLE_VISUALIZATION:
                        cv2.rectangle(display_img, (tx1, ty1), (tx2, ty2), (0, 255, 255), 5)
                        cv2.putText(display_img, f"TARGET {target.distance:.1f}m", (tx1, max(ty1 - 15, 20)),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

            # Vis GUI-vinduer utelukkende dersom visualisering er aktivert via konfigurasjonsvariabelen
            if ENABLE_VISUALIZATION:
                cv2.drawMarker(display_img, (w // 2, h // 2), (0, 255, 255), cv2.MARKER_CROSS, 30, 2)
                cv2.imshow("Webcam - YOLO Tracking Stream", display_img)
                if cv2.waitKey(1) == ord("q"):
                    break
                    
        # Hvis visualisering er av, kan du legge inn en mikroskopisk sleep for å unngå 100% trådlåsing
        # f.eks. ved bruk av standard transport-strømmer eller tidsavbrudd.

    finally:
        if using_realsense and rs_pipeline is not None:
            rs_pipeline.stop()
        elif cap is not None:
            close_webcam(cap)
        if ENABLE_VISUALIZATION:
            cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
