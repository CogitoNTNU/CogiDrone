"""Sanntids pose estimation med YOLO over webcam."""

from ultralytics import YOLO
import cv2

from camera import open_webcam, close_webcam

MODEL_WEIGHTS = "yolov8n-pose.pt"

# Par av keypoint-indekser som skal tegnes som linjer mellom hverandre
SKELETON = [
    (5, 7), (7, 9),      # left arm
    (6, 8), (8, 10),     # right arm
    (11, 13), (13, 15),  # left leg
    (12, 14), (14, 16),  # right leg
    (5, 6), (11, 12),    # shoulders and hips
    (5, 11), (6, 12),    # torso
]


def main() -> None:
    cap = open_webcam()
    model = YOLO(MODEL_WEIGHTS)

    while True:
        success, img = cap.read()
        if not success:
            continue

        results = model(img, stream=True)

        for r in results:
            if r.keypoints is None:  # skip if no keypoints detected
                continue

            kps = r.keypoints.xy.cpu().numpy()  # shape: (num_objects, num_keypoints, 2)

            for obj_kps in kps:
                for x, y in obj_kps:
                    if x > 0 and y > 0:  # valid point
                        cv2.circle(img, (int(x), int(y)), 4, (0, 255, 0), -1)

            for obj_kps in kps:
                for i, j in SKELETON:
                    x1, y1 = obj_kps[i]
                    x2, y2 = obj_kps[j]
                    if x1 > 0 and y1 > 0 and x2 > 0 and y2 > 0:
                        cv2.line(img, (int(x1), int(y1)), (int(x2), int(y2)), (255, 0, 0), 2)

        cv2.imshow("YOLO Pose Estimation", img)
        if cv2.waitKey(1) == ord("q"):
            break

    close_webcam(cap)


if __name__ == "__main__":
    main()
