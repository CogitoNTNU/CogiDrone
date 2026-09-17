"""Sanntids objektdeteksjon og dybdeestimering med YOLO og Depth Anything V2."""

import cv2
import numpy as np
import torch
from PIL import Image
from transformers import pipeline
from ultralytics import YOLO

from camera import open_webcam, close_webcam, COCO_CLASS_NAMES

MODEL_WEIGHTS = "yolov8n.pt"
AVERAGE_HUMAN_HEIGHT_M = 1.70


def main() -> None:
    cap = open_webcam()
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

    frame_count = 0
    cached_depth_map = None
    depth_interval = 2  # run depth model per x frame

    while True:
        success, img = cap.read()
        if not success:
            continue

        frame_count += 1
        h, w, _ = img.shape
        focal_length = w * 0.8 #based on the camera

        results = model(img, stream=True, verbose=False)

        # Depth estimation
        if frame_count % depth_interval == 1 or cached_depth_map is None:
            pil_img = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
            depth_output = depth_pipe(pil_img)
            raw_depth = np.array(depth_output["depth"])
            cached_depth_map = cv2.resize(raw_depth, (w, h))

        for r in results:
            for box in r.boxes:
                x1, y1, x2, y2 = map(int, box.xyxy[0])
                cv2.rectangle(img, (x1, y1), (x2, y2), (255, 0, 255), 3)

                cls = int(box.cls[0])
                cls_name = COCO_CLASS_NAMES[cls] if cls < len(COCO_CLASS_NAMES) else "objekt"

                #Distance calc if person
                bbox_h = max(y2 - y1, 1)
                geom_dist = (AVERAGE_HUMAN_HEIGHT_M * focal_length) / bbox_h if cls_name == "person" else 0.0

                crop = cached_depth_map[y1:y2, x1:x2]
                rel_depth = float(np.median(crop)) if crop.size > 0 else 0.0

                if cls_name == "person":
                    label = f"{cls_name} | ~{geom_dist:.1f}m"
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