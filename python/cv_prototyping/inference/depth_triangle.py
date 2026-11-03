"""Sanntids objektdeteksjon og avstand med triangulering (a² + b² = c²) og targeting.

Samme oppsett som depth_head.py, men avstanden til personer beregnes ikke fra
størrelsen på bounding-boksen (varierer for mye). I stedet bruker vi en rettvinklet
trekant mellom kameraet og punktet på bakken under personen:

        kamera
          |\\
        a | \\ c   a = kameraets høyde over målpunktet (kjent)
          |  \\     b = horisontal avstand til målpunktet
          |___\\    c = siktlinjen kamera -> målpunktet
               punkt

  - Målpunktet er midten av bounding-boksen, som antas å ligge TARGET_POINT_HEIGHT_M
    over bakken. a = kamerahøyde - TARGET_POINT_HEIGHT_M.
  - Vinkelen α under horisonten = kamera-pitch + atan((cy_box - cy) / fy).
  - c = a / sin(α)  og  b = a / tan(α), som oppfyller a² + b² = c².

Antakelser: flatt terreng, kjent høyde over bakken (AGL), kjent pitch og at hele
personen er synlig (ellers flytter boksmidten seg og estimatet blir feil).
Taster: w/s = pitch +/- 0.5°, e/d = høyde +/- 5 cm, q = avslutt.
"""

import math
import sys
from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np
import torch
from PIL import Image
from transformers import pipeline
from ultralytics import YOLO

from camera import open_webcam, close_webcam, COCO_CLASS_NAMES

# Gjør targeter/-mappen importerbar (ligger ved siden av inference/)
sys.path.append(str(Path(__file__).resolve().parent.parent))
from targeter.target_closest import ClosestTargeter, Detection, is_too_close

MODEL_WEIGHTS = "weights/yolo26n.pt" #"models/yolo26n.pt"
PERSON_CONF_THRESHOLD = 0.10
OTHER_CONF_THRESHOLD = 0.25

AVERAGE_HUMAN_HEIGHT_M = 1.70  # Kun fallback når trekantmetoden ikke er pålitelig
TARGET_POINT_HEIGHT_M = AVERAGE_HUMAN_HEIGHT_M / 2  # Boksmidten antas å ligge midt på kroppen

# Under denne vinkelen (grader under horisonten) gir én pikselfeil stor avstandsfeil
MIN_DEPRESSION_DEG = 2.0

# Verdier for testing med webkamera på stativ (overstyres av dronen senere)
TEST_CAMERA_HEIGHT_M = 1.5
TEST_CAMERA_PITCH_DEG = 10.0  # Positiv = kamera vinklet nedover


@dataclass
class TriangleEstimate:
    ground_dist_m: float    # b: horisontal avstand til målpunktet
    slant_dist_m: float     # c: siktlinje kamera -> målpunktet
    depression_deg: float   # α: vinkel under horisonten


def estimate_distance(point_v: float, height_m: float, pitch_deg: float, fy: float, cy: float) -> TriangleEstimate | None:
    """Beregn avstand til et punkt i bildelinje point_v med a² + b² = c².

    Punktet antas å ligge TARGET_POINT_HEIGHT_M over bakken.
    Returnerer None hvis siktlinjen peker over horisonten, eller punktet ligger
    i samme høyde som eller over kameraet.
    """
    pixel_angle = math.atan((point_v - cy) / fy)   # vinkel under optisk akse
    alpha = math.radians(pitch_deg) + pixel_angle  # vinkel under horisonten
    a = height_m - TARGET_POINT_HEIGHT_M           # loddrett fall fra kamera til punktet
    if alpha <= 1e-6 or a <= 1e-6:
        return None

    c = a / math.sin(alpha)
    b = math.sqrt(max(c * c - a * a, 0.0))
    return TriangleEstimate(b, c, math.degrees(alpha))


def offset_from_center(x1, y1, x2, y2, frame_w, frame_h):
    """Returnerer (dx, dy) i piksler fra skjermens midte til midten av boksen.
    dx > 0: target er til høyre for midten
    dy > 0: target er under midten (bildekoordinater, y går nedover)
    """
    cx = (x1 + x2) / 2
    cy = (y1 + y2) / 2
    dx = cx - frame_w / 2
    dy = cy - frame_h / 2
    return dx, dy


def main() -> None:
    cap = open_webcam(width=640, height=480, camera_index=1)
    model = YOLO(MODEL_WEIGHTS)

    print("Laster inn Depth Anything V2...")
    device = "cuda" if torch.cuda.is_available() else "cpu"
    torch_dtype = torch.float16 if device == "cuda" else torch.float32

    depth_pipe = pipeline(
        task="depth-estimation",
        model="depth-anything/Depth-Anything-V2-Small-hf",
        torch_dtype=torch_dtype,
        device=0 if device == "cuda" else -1
    )
    print("Modeller klare!")

    targeter = ClosestTargeter()

    height_m = TEST_CAMERA_HEIGHT_M
    pitch_deg = TEST_CAMERA_PITCH_DEG

    frame_count = 0
    cached_depth_map = None
    depth_interval = 2  # run depth model per x frame

    while True:
        success, img = cap.read()
        if not success:
            continue

        frame_count += 1
        h, w, _ = img.shape
        focal_length = w * 0.8  # Focal length dynamically scaled to webcam frame width

        # Primary YOLO detection on full frame
        results = model(
            img, imgsz=640, conf=PERSON_CONF_THRESHOLD, stream=True, verbose=False
        )

        # Depth estimation
        if frame_count % depth_interval == 1 or cached_depth_map is None:
            pil_img = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
            depth_output = depth_pipe(pil_img)
            raw_depth = np.array(depth_output["depth"])
            cached_depth_map = cv2.resize(raw_depth, (w, h))

        detections = []  # Alle personer i denne framen, sendes til targeteren
        for r in results:
            for box in r.boxes:
                x1, y1, x2, y2 = map(int, box.xyxy[0])
                cls = int(box.cls[0])
                confidence = float(box.conf[0])
                if cls != 0 and confidence < OTHER_CONF_THRESHOLD:
                    continue
                if cls < len(COCO_CLASS_NAMES):
                    cls_name = COCO_CLASS_NAMES[cls]
                else:
                    continue  # Skip unknown classes

                est = None
                triangle_ok = False
                body_dist = 0.0

                if cls_name == "person":
                    # Trekantberegning fra midten av boksen
                    est = estimate_distance((y1 + y2) / 2.0, height_m, pitch_deg, focal_length, h / 2.0)
                    # Er personen kuttet av bildekanten (topp eller bunn), er boksmidten ikke midt på kroppen
                    body_cut = y1 <= 1 or y2 >= h - 2
                    triangle_ok = est is not None and not body_cut and est.depression_deg >= MIN_DEPRESSION_DEG

                    # Kroppshøyde brukes kun som fallback når trekanten ikke er pålitelig
                    bbox_h = max(y2 - y1, 1)
                    body_dist = (AVERAGE_HUMAN_HEIGHT_M * focal_length) / bbox_h

                    distance = est.slant_dist_m if triangle_ok else body_dist
                    detections.append(Detection((x1, y1, x2, y2), confidence, distance))

                # Relative depth from Depth Anything V2
                crop_depth = cached_depth_map[y1:y2, x1:x2]
                rel_depth = float(np.median(crop_depth)) if crop_depth.size > 0 else 0.0

                # Draw main person bounding box in MAGENTA
                cv2.rectangle(img, (x1, y1), (x2, y2), (255, 0, 255), 3)

                dx, dy = offset_from_center(x1, y1, x2, y2, w, h)
                box_cx, box_cy = (x1 + x2) // 2, (y1 + y2) // 2

                # Linje fra bildets midte til midten av boksen
                cv2.line(img, (w // 2, h // 2), (box_cx, box_cy), (0, 255, 0), 2)

                # dx, dy som tekst under boksen
                cv2.putText(
                    img, f"dx={dx:.0f} dy={dy:.0f}", (x1, min(y2 + 25, h - 10)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2
                    )

                # Format text label dynamically
                if cls_name == "person":
                    cv2.circle(img, (box_cx, box_cy), 4, (0, 255, 0), -1)
                    if triangle_ok:
                        label = f"Person | Slant c: {est.slant_dist_m:.1f}m | Ground b: {est.ground_dist_m:.1f}m"
                    else:
                        label = f"Person | Body (fallback): ~{body_dist:.1f}m"
                else:
                    label = f"{cls_name} | Rel: {rel_depth:.0f}"

                cv2.putText(
                    img, label, (x1, max(y1 - 10, 20)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 0), 2
                )

        # Velg og marker target: gul boks, rød hvis for nær
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

        # Kryss i bildets midte
        cv2.drawMarker(img, (w // 2, h // 2), (0, 255, 255), cv2.MARKER_CROSS, 30, 2)
        status = f"Hoyde: {height_m:.2f}m  Pitch: {pitch_deg:.1f}"
        cv2.putText(img, status, (10, h - 15), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        cv2.imshow("Webcam - YOLO + Depth Anything V2 + Trekant", img)
        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break
        elif key == ord("w"):
            pitch_deg += 0.5
        elif key == ord("s"):
            pitch_deg -= 0.5
        elif key == ord("e"):
            height_m += 0.05
        elif key == ord("d"):
            height_m = max(0.05, height_m - 0.05)

    close_webcam(cap)


if __name__ == "__main__":
    main()
