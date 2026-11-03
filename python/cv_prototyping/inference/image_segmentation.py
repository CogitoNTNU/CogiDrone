"""Sanntids instanssegmentering med YOLO over webcam."""

from ultralytics import YOLO
import cv2

from camera import open_webcam, close_webcam, COCO_CLASS_NAMES

MODEL_WEIGHTS = "yolov8n-seg.pt"


def main() -> None:
    cap = open_webcam()
    model = YOLO(MODEL_WEIGHTS)

    while True:
        success, img = cap.read()
        if not success:
            continue

        results = model(img, stream=True)

        for r in results:
            masks = r.masks
            boxes = r.boxes
            if masks is None:  # Skip if no masks detected
                continue

            for mask, box in zip(masks, boxes):
                mask = mask.xy[0].astype(int)

                cls = int(box.cls[0])
                # Draw filled mask overlay
                color = (0, 255, 0) if cls == 0 else (255, 0, 0)
                overlay = img.copy()
                cv2.fillPoly(overlay, [mask], color=color)
                img = cv2.addWeighted(overlay, 0.5, img, 0.5, 0)

                # object details
                org = (int(box.xyxy[0][0]), int(box.xyxy[0][1]))
                cv2.putText(img, COCO_CLASS_NAMES[cls], org,
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 0, 0), 2)

        cv2.imshow("Webcam", img)
        if cv2.waitKey(1) == ord("q"):
            break

    close_webcam(cap)


if __name__ == "__main__":
    main()
