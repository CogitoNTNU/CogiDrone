"""Sanntids objektdeteksjon med YOLO over webcam."""

from ultralytics import YOLO
import cv2

from camera import open_webcam, close_webcam, COCO_CLASS_NAMES

MODEL_WEIGHTS = "yolov8n.pt"


def main() -> None:
    cap = open_webcam()
    model = YOLO(MODEL_WEIGHTS)

    while True:
        success, img = cap.read()
        if not success:
            continue

        results = model(img, stream=True)

        for r in results:
            for box in r.boxes:
                # bounding box
                x1, y1, x2, y2 = map(int, box.xyxy[0])
                cv2.rectangle(img, (x1, y1), (x2, y2), (255, 0, 255), 3)

                # class name
                cls = int(box.cls[0])
                cv2.putText(img, COCO_CLASS_NAMES[cls], (x1, y1),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 0, 0), 2)

        cv2.imshow("Webcam", img)
        if cv2.waitKey(1) == ord("q"):
            break

    close_webcam(cap)


if __name__ == "__main__":
    main()
