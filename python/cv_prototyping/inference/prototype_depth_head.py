import math
import os
import cv2
import numpy as np
import torch
from PIL import Image
from transformers import pipeline
from ultralytics import YOLO

# ---------------------------------------------------------
# Configurations
# ---------------------------------------------------------
image_path = "./python/cv_prototyping/inference/img/img.png"
detection_output_path = "./python/cv_prototyping/inference/img/output.png"
depth_output_path = "./python/cv_prototyping/inference/img/depth_output.png"

PERSON_CLASS_ID = 0

# Real-world object height estimates in meters
AVERAGE_HEAD_HEIGHT_M = 0.23    # Average adult head height (~23cm)
AVERAGE_HUMAN_HEIGHT_M = 1.7    # Fallback if head is not detected
FOCAL_LENGTH_PX = 3200.0        # Estimated camera focal length in pixels

# Camera Geometry Constants
CAMERA_HEIGHT_M = 1.5           # Height of camera above ground in meters
CAMERA_PITCH_DEG = 10.0         # Camera tilt angle down from horizontal (degrees)

CONF_THRESHOLD = 0.25           # Minimum confidence score

if not os.path.exists(image_path):
    raise FileNotFoundError(f"Image not found at path: {image_path}")

img = cv2.imread(image_path)
if img is None:
    raise ValueError(f"Failed to load image at {image_path}")

img_h, img_w, _ = img.shape
annotated_img = img.copy()

# ---------------------------------------------------------
# 1. Setup Device & Initialize Depth Pipeline
# ---------------------------------------------------------
device = "cuda" if torch.cuda.is_available() else "cpu"
torch_dtype = torch.float16 if device == "cuda" else torch.float32

print(f"Using device: {device} for Depth Anything V2...")
depth_pipe = pipeline(
    task="depth-estimation",
    model="depth-anything/Depth-Anything-V2-Large-hf",
    torch_dtype=torch_dtype,
    device=0 if device == "cuda" else -1,
)

# ---------------------------------------------------------
# 2. Generate Depth Map Array
# ---------------------------------------------------------
pil_img = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))

print("Running depth estimation...")
depth_result = depth_pipe(pil_img)

raw_depth_pil = depth_result["depth"]
raw_depth_map = cv2.resize(np.array(raw_depth_pil), (img_w, img_h))

depth_color = cv2.applyColorMap(raw_depth_map, cv2.COLORMAP_INFERNO)
cv2.imwrite(depth_output_path, depth_color)
print(f"Saved depth visualization to {depth_output_path}")

# ---------------------------------------------------------
# 3. Object Detection Setup (Standard YOLO)
# ---------------------------------------------------------
person_model = YOLO("yolo26n.pt")
head_model = YOLO("medium.pt")

# ---------------------------------------------------------
# 4. Standard Person Detection (Native Resolution)
# ---------------------------------------------------------
person_results = person_model(img, conf=CONF_THRESHOLD)

if person_results and len(person_results) > 0 and person_results[0].boxes is not None:
    boxes = person_results[0].boxes
    
    cy = img_h / 2.0  # Vertical optical center of image
    pitch_rad = math.radians(CAMERA_PITCH_DEG)

    for box in boxes:
        cls_id = int(box.cls[0].item())
        if cls_id != PERSON_CLASS_ID:
            continue

        x1, y1, x2, y2 = box.xyxy[0].cpu().numpy().astype(int)
        conf = float(box.conf[0].item())
        w, h = x2 - x1, y2 - y1

        # ---------------------------------------------------------
        # 5. Secondary Pass: Detect Head on Crop
        # ---------------------------------------------------------
        crop_x1, crop_y1 = max(0, x1), max(0, y1)
        crop_x2, crop_y2 = min(img_w, x2), min(img_h, y2)

        person_crop = img[crop_y1:crop_y2, crop_x1:crop_x2]

        head_detected = False
        geom_dist = 0.0

        if person_crop.size > 0:
            head_results = head_model(person_crop)

            if head_results and len(head_results) > 0 and head_results[0].boxes is not None:
                head_boxes = head_results[0].boxes

                if len(head_boxes) > 0:
                    best_head_idx = int(torch.argmax(head_boxes.conf).item())
                    h_box = head_boxes[best_head_idx]

                    hx1, hy1, hx2, hy2 = h_box.xyxy[0].cpu().numpy().astype(int)

                    global_hx1 = crop_x1 + hx1
                    global_hy1 = crop_y1 + hy1
                    global_hx2 = crop_x1 + hx2
                    global_hy2 = crop_y1 + hy2

                    # Distance calculated using native head bounding box height
                    head_h = max(hy2 - hy1, 1)
                    geom_dist = (AVERAGE_HEAD_HEIGHT_M * FOCAL_LENGTH_PX) / head_h
                    head_detected = True

                    # Draw Head bounding box (Red)
                    cv2.rectangle(
                        annotated_img,
                        (global_hx1, global_hy1),
                        (global_hx2, global_hy2),
                        (0, 0, 255),
                        2,
                    )

        # Fallback to native full-body height if no head detected
        if not head_detected:
            bbox_h = max(h, 1)
            geom_dist = (AVERAGE_HUMAN_HEIGHT_M * FOCAL_LENGTH_PX) / bbox_h

        # ---------------------------------------------------------
        # 6. Calculate Distance via Camera Height & Pitch Angle
        # ---------------------------------------------------------
        # y2 is the bottom of the bounding box (where the feet touch the ground)
        pixel_angle_rad = math.atan((y2 - cy) / FOCAL_LENGTH_PX)
        total_angle_rad = pitch_rad + pixel_angle_rad

        # Calculate ground distance (avoid divide-by-zero or negative angle issues)
        if total_angle_rad > 0:
            pitch_dist = CAMERA_HEIGHT_M / math.tan(total_angle_rad)
        else:
            pitch_dist = 0.0

        # Draw Person bounding box (Green)
        cv2.rectangle(annotated_img, (x1, y1), (x2, y2), (0, 255, 0), 2)

        # Draw unified label with both distance calculations
        person_label = f"Person | ~{geom_dist:.1f}m | Pitch: ~{pitch_dist:.1f}m | Acc: {conf:.2f}"
        cv2.putText(
            annotated_img,
            person_label,
            (x1, max(y1 - 10, 20)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.6,
            (0, 255, 0),
            2,
        )

# ---------------------------------------------------------
# 7. Save Output
# ---------------------------------------------------------
cv2.imwrite(detection_output_path, annotated_img)
print(f"Saved output to {detection_output_path}")