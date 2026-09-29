"""Sanntids objektdeteksjon og dybdeestimering med YOLO og Depth Anything V2."""

import cv2
import numpy as np
import torch
from PIL import Image
from transformers import pipeline
from ultralytics import YOLO

from camera import open_webcam, close_webcam, COCO_CLASS_NAMES

MODEL_WEIGHTS = "models/yolo26n.pt"
HEAD_MODEL_WEIGHTS = "models/medium.pt"  # Your head detection model

AVERAGE_HUMAN_HEIGHT_M = 1.70
AVERAGE_HEAD_HEIGHT_M = 0.23      # Average human head height (~23cm)


def main() -> None:
    cap = open_webcam(width=1280, height=720, camera_index=1)
    
    # Load both models once before the live loop
    model = YOLO(MODEL_WEIGHTS)
    head_model = YOLO(HEAD_MODEL_WEIGHTS)

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
        results = model(img, imgsz=1280, stream=True, verbose=False)

        # Depth estimation
        if frame_count % depth_interval == 1 or cached_depth_map is None:
            pil_img = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
            depth_output = depth_pipe(pil_img)
            raw_depth = np.array(depth_output["depth"])
            cached_depth_map = cv2.resize(raw_depth, (w, h))

        for r in results:
            for box in r.boxes:
                x1, y1, x2, y2 = map(int, box.xyxy[0])
                cls = int(box.cls[0])
                cls_name = COCO_CLASS_NAMES[cls] if cls < len(COCO_CLASS_NAMES) else "objekt"

                geom_dist = 0.0
                head_dist = None

                if cls_name == "person":
                    # 1. Primary calculation: Full body distance
                    bbox_h = max(y2 - y1, 1)
                    geom_dist = (AVERAGE_HUMAN_HEIGHT_M * focal_length) / bbox_h

                    # 2. Secondary pass: Head detection inside person crop
                    crop_x1, crop_y1 = max(0, x1), max(0, y1)
                    crop_x2, crop_y2 = min(w, x2), min(h, y2)
                    person_crop = img[crop_y1:crop_y2, crop_x1:crop_x2]

                    if person_crop.size > 0:
                        head_res = head_model(person_crop, imgsz=1280, verbose=False)
                        if head_res and len(head_res[0].boxes) > 0:
                            # Pick head with highest confidence score
                            best_head_idx = int(torch.argmax(head_res[0].boxes.conf).item())
                            h_box = head_res[0].boxes[best_head_idx]
                            
                            hx1, hy1, hx2, hy2 = map(int, h_box.xyxy[0].cpu().numpy())

                            # Map local crop coordinates back to full image space
                            global_hx1 = crop_x1 + hx1
                            global_hy1 = crop_y1 + hy1
                            global_hx2 = crop_x1 + hx2
                            global_hy2 = crop_y1 + hy2

                            # Head height in pixels
                            head_h = max(hy2 - hy1, 1)

                            # Calculate distance using head size & same focal length scaling
                            head_dist = (AVERAGE_HEAD_HEIGHT_M * focal_length) / head_h

                            # Draw head bounding box in RED
                            cv2.rectangle(img, (global_hx1, global_hy1), (global_hx2, global_hy2), (0, 0, 255), 2)

                # Relative depth from Depth Anything V2
                crop_depth = cached_depth_map[y1:y2, x1:x2]
                rel_depth = float(np.median(crop_depth)) if crop_depth.size > 0 else 0.0

                # Draw main person bounding box in MAGENTA
                cv2.rectangle(img, (x1, y1), (x2, y2), (255, 0, 255), 3)

                # Format text label dynamically
                if cls_name == "person":
                    if head_dist is not None:
                        label = f"Person | Body: ~{geom_dist:.1f}m | Head: ~{head_dist:.1f}m"
                    else:
                        label = f"Person | Body: ~{geom_dist:.1f}m"
                else:
                    label = f"{cls_name} | Rel: {rel_depth:.0f}"

                cv2.putText(
                    img, label, (x1, max(y1 - 10, 20)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 0), 2
                )

        cv2.imshow("Webcam - YOLO + Depth Anything V2", img)
        if cv2.waitKey(1) == ord("q"):
            break

    close_webcam(cap)


if __name__ == "__main__":
    main()