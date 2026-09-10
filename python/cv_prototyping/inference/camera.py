"""Delte hjelpefunksjoner for cv_prototyping-skriptene (webcam + YOLO-klassenavn)."""

import cv2

# Klassenavnene til de forhåndstrente YOLO-modellene (COCO-datasettet)
COCO_CLASS_NAMES = [
    "person", "bicycle", "car", "motorbike", "aeroplane", "bus", "train", "truck", "boat",
    "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat",
    "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella",
    "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite", "baseball bat",
    "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
    "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli",
    "carrot", "hot dog", "pizza", "donut", "cake", "chair", "sofa", "pottedplant", "bed",
    "diningtable", "toilet", "tvmonitor", "laptop", "mouse", "remote", "keyboard", "cell phone",
    "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors",
    "teddy bear", "hair drier", "toothbrush",
]


def open_webcam(width: int = 640, height: int = 480, camera_index: int = 0) -> cv2.VideoCapture:
    """Åpne webcam med gitt oppløsning."""
    cap = cv2.VideoCapture(camera_index)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    return cap


def close_webcam(cap: cv2.VideoCapture) -> None:
    """Frigi kameraet og lukk OpenCV-vinduer."""
    cap.release()
    cv2.destroyAllWindows()
