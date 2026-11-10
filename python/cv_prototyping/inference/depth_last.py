"""Real-time object detection and distance estimation for 6 S&R classes with YOLO and RealSense hardware depth."""

import sys
from pathlib import Path

import cv2
import numpy as np
import torch
from ultralytics import YOLO

# Import camera functions and checks from your updated camera.py
from camera import open_webcam, close_webcam, is_realsense_available
import pyrealsense2 as rs

# Make the targeter folder importable (located next to inference/)
sys.path.append(str(Path(__file__).resolve().parent.parent))
from targeter.target_closest import ClosestTargeter, Detection

# ==================== CONFIGURATION ====================
MODEL_WEIGHTS = "models/yolo26n.pt" #"models/best.pt" 
HEAD_MODEL_WEIGHTS = "models/medium.pt"
CAMERA_INDEX = 0
CONF_THRESHOLD = 0.20

# Controls whether windows and GUI drawings should be shown (Set to True for debugging)
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
AVERAGE_HEAD_HEIGHT_M = 0.23 # Average head height (~23cm)

def offset_from_center(x1, y1, x2, y2, frame_w, frame_h):
    """Returns (dx, dy) in pixels from the screen center to the center of the box."""
    cx = (x1 + x2) / 2
    cy = (y1 + y2) / 2
    dx = cx - frame_w / 2
    dy = cy - frame_h / 2
    return dx, dy


def main() -> None:
    # Initialize YOLO models
    model = YOLO(MODEL_WEIGHTS)
    head_model = YOLO(HEAD_MODEL_WEIGHTS)
    
    # Check camera type and set up hardware stream
    using_realsense = is_realsense_available()
    
    cap = None
    rs_pipeline = None 
    align = None
    
    if using_realsense:
        print("RealSense camera found! Starting hardware pipeline...")
        rs_pipeline = rs.pipeline()
        config = rs.config()
        config.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
        config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)
        rs_pipeline.start(config)
        
        # Ensure depth map and RGB image are 100% pixel-aligned in space
        align = rs.align(rs.stream.color)
    else:
        print("No RealSense found. Using standard webcam...")
        cap = open_webcam(width=640, height=480, camera_index=CAMERA_INDEX)
    
    print("Models and camera ready!")
    targeter = ClosestTargeter()

    try:
        while True:
            depth_frame = None
            
            # Retrieve raw frames based on active camera type
            if using_realsense:
                frames = rs_pipeline.wait_for_frames()
                # Align depth to the RGB perspective
                aligned_frames = align.process(frames)
                
                depth_frame = aligned_frames.get_depth_frame()
                color_frame = aligned_frames.get_color_frame()
                
                if not depth_frame or not color_frame:
                    continue
                img = np.asanyarray(color_frame.get_data())
            else:
                success, img = cap.read()
                if not success:
                    continue

            h, w, _ = img.shape
            focal_length = w * 0.8 # Scaled focal length based on width
            
            # Create a copy for visual overlays (fixed from .clone() to .copy())
            display_img = img.copy() if ENABLE_VISUALIZATION else None

            # Primary YOLO detection runs exclusively on the clean RGB image
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
                    
                    # Center point for the object
                    box_cx = int((x1 + x2) / 2)
                    box_cy = int((y1 + y2) / 2)
                    
                    # Ensure center point is within valid boundaries
                    box_cx = max(0, min(box_cx, w - 1))
                    box_cy = max(0, min(box_cy, h - 1))

                    # Calculate positional offset from screen center
                    dx, dy = offset_from_center(x1, y1, x2, y2, w, h)

                    # --- DISTANCE CALCULATION STRATEGY ---
                    distance = None
                    distance_source = None
                    
                    # 1. Priority: RealSense hardware depth (only within 15 meters)
                    if using_realsense and depth_frame:
                        hardware_dist = depth_frame.get_distance(box_cx, box_cy)
                        if 0.1 < hardware_dist <= 15.0:
                            distance = hardware_dist
                            distance_source = "realsense_laser"

                    # 2. Priority: Head geometry estimation (if RealSense is absent, >15m, or noisy)
                    if distance is None:
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
                                distance = (AVERAGE_HEAD_HEIGHT_M * focal_length) / head_h
                                distance_source = "head_geometry"
                                
                                if ENABLE_VISUALIZATION:
                                    global_hx1 = crop_x1 + hx1
                                    global_hy1 = crop_y1 + hy1
                                    global_hx2 = crop_x1 + hx2
                                    global_hy2 = crop_y1 + hy2
                                    cv2.rectangle(display_img, (global_hx1, global_hy1), (global_hx2, global_hy2), (0, 0, 255), 2)

                    # 3. Last fallback: Full body geometry (when head is not available)
                    if distance is None:
                        bbox_h = max(y2 - y1, 1)
                        distance = (AVERAGE_HUMAN_HEIGHT_M * focal_length) / bbox_h
                        distance_source = "body_geometry"

                    # --- PROXIMITY CHECK & WARNING (< 5 METERS) ---
                    if distance <= 5.0:
                        # Calculate relative angle/heading in degrees from camera centerline
                        angle_rad = np.arctan(dx / focal_length)
                        angle_deg = np.degrees(angle_rad)
                        print(f"⚠️ WARNING: Object '{cls_name}' within 5m! Distance: {distance:.2f}m | Heading/Angle: {angle_deg:+.1f}° (dx: {dx:.0f}px) | Method: {distance_source}")

                    # Register detection
                    detections.append(Detection((x1, y1, x2, y2), confidence, distance))
                    
                    # Save measurement data to telemetry log
                    detected_targets_log.append({
                        "class": cls_name,
                        "distance_m": round(distance, 2),
                        "offset_dx": round(dx, 1),
                        "offset_dy": round(dy, 1),
                        "source": distance_source
                    })

                    # Debug drawings on display_img
                    if ENABLE_VISUALIZATION:
                        cv2.rectangle(display_img, (x1, y1), (x2, y2), (255, 0, 255), 3)
                        cv2.line(display_img, (w // 2, h // 2), (box_cx, box_cy), (0, 255, 0), 2)
                        cv2.putText(display_img, f"dx={dx:.0f} dy={dy:.0f}", (x1, min(y2 + 25, h - 10)),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

            if ENABLE_VISUALIZATION and display_img is not None:
                cv2.imshow("Drone Detection", display_img)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break

    finally:
        if cap:
            close_webcam(cap)
        if rs_pipeline:
            rs_pipeline.stop()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()