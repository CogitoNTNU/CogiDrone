"""Sanntids objektdeteksjon, pose-basert avstandsmåling og dybdeestimering."""

import cv2
import numpy as np
import torch
from PIL import Image
from transformers import pipeline
from ultralytics import YOLO

from camera import open_webcam, close_webcam, COCO_CLASS_NAMES

MODEL_WEIGHTS = "yolov8n.pt"
POSE_MODEL_WEIGHTS = "yolov8n-pose.pt"
AVERAGE_HUMAN_HEIGHT_M = 1.70
AVERAGE_SHOULDER_WIDTH_M = 0.40
AVERAGE_EYE_DISTANCE_M = 0.063

# COCO pose keypoint-indekser
LEFT_EYE, RIGHT_EYE = 1, 2
LEFT_SHOULDER, RIGHT_SHOULDER = 5, 6

# Par av keypoint-indekser som skal tegnes som linjer mellom hverandre
SKELETON = [
    (5, 7), (7, 9),      # left arm
    (6, 8), (8, 10),     # right arm
    (11, 13), (13, 15),  # left leg
    (12, 14), (14, 16),  # right leg
    (5, 6), (11, 12),    # shoulders and hips
    (5, 11), (6, 12),    # torso
]


def _valid(pt) -> bool:
    return pt[0] > 0 and pt[1] > 0


def _keypoint_distance_m(pt_a, pt_b, real_world_m: float, focal_length: float) -> float | None:
    if not (_valid(pt_a) and _valid(pt_b)):
        return None
    pixel_dist = float(np.hypot(pt_b[0] - pt_a[0], pt_b[1] - pt_a[1]))
    if pixel_dist <= 0:
        return None
    return (real_world_m * focal_length) / pixel_dist


def estimate_person_distance(obj_kps: np.ndarray, bbox_h: int, focal_length: float) -> tuple[float, str]:
    """Kombiner øye- og skulderavstand til ett estimat; faller tilbake til bbox-høyde hvis ingen er synlige."""
    eye_dist = _keypoint_distance_m(obj_kps[LEFT_EYE], obj_kps[RIGHT_EYE], AVERAGE_EYE_DISTANCE_M, focal_length)
    shoulder_dist = _keypoint_distance_m(
        obj_kps[LEFT_SHOULDER], obj_kps[RIGHT_SHOULDER], AVERAGE_SHOULDER_WIDTH_M, focal_length
    )

    parts, estimates = [], []
    if eye_dist is not None:
        parts.append("eyes")
        estimates.append(eye_dist)
    if shoulder_dist is not None:
        parts.append("shoulders")
        estimates.append(shoulder_dist)

    if estimates:
        return sum(estimates) / len(estimates), "+".join(parts)

    return (AVERAGE_HUMAN_HEIGHT_M * focal_length) / bbox_h, "bbox"


def draw_pose(img, obj_kps: np.ndarray) -> None:
    for x, y in obj_kps:
        if x > 0 and y > 0:
            cv2.circle(img, (int(x), int(y)), 4, (0, 255, 0), -1)

    for i, j in SKELETON:
        x1, y1 = obj_kps[i]
        x2, y2 = obj_kps[j]
        if x1 > 0 and y1 > 0 and x2 > 0 and y2 > 0:
            cv2.line(img, (int(x1), int(y1)), (int(x2), int(y2)), (255, 0, 0), 2)


def main() -> None:
    cap = open_webcam()
    model = YOLO(MODEL_WEIGHTS)
    pose_model = YOLO(POSE_MODEL_WEIGHTS)

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
        focal_length = w * 0.8  # based on the camera

        results = model(img, stream=True, verbose=False)
        pose_results = pose_model(img, stream=True, verbose=False)

        # Depth estimation
        if frame_count % depth_interval == 1 or cached_depth_map is None:
            pil_img = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
            depth_output = depth_pipe(pil_img)
            raw_depth = np.array(depth_output["depth"])
            cached_depth_map = cv2.resize(raw_depth, (w, h))

        # Personer dekkes av pose-modellen under; her tegnes bare øvrige klasser
        for r in results:
            for box in r.boxes:
                cls = int(box.cls[0])
                cls_name = COCO_CLASS_NAMES[cls] if cls < len(COCO_CLASS_NAMES) else "objekt"
                if cls_name == "person":
                    continue

                x1, y1, x2, y2 = map(int, box.xyxy[0])
                cv2.rectangle(img, (x1, y1), (x2, y2), (255, 0, 255), 3)

                crop = cached_depth_map[y1:y2, x1:x2]
                rel_depth = float(np.median(crop)) if crop.size > 0 else 0.0
                label = f"{cls_name} | Rel: {rel_depth:.0f}"

                cv2.putText(
                    img, label, (x1, max(y1 - 10, 20)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 0), 2
                )

        for r in pose_results:
            if r.keypoints is None or r.boxes is None:
                continue

            boxes = r.boxes.xyxy.cpu().numpy()
            kps = r.keypoints.xy.cpu().numpy()

            for box, obj_kps in zip(boxes, kps):
                x1, y1, x2, y2 = map(int, box)
                cv2.rectangle(img, (x1, y1), (x2, y2), (255, 0, 255), 3)

                bbox_h = max(y2 - y1, 1)
                distance_m, source = estimate_person_distance(obj_kps, bbox_h, focal_length)
                label = f"person | ~{distance_m:.2f}m ({source})"

                cv2.putText(
                    img, label, (x1, max(y1 - 10, 20)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 0), 2
                )
                draw_pose(img, obj_kps)

        cv2.imshow("Webcam - YOLO + Pose + Depth Anything V2", img)
        if cv2.waitKey(1) == ord("q"):
            break

    close_webcam(cap)


if __name__ == "__main__":
    main()
