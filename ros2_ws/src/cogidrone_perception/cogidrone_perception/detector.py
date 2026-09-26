"""YOLO detector node.

Subscribes to a camera image, runs YOLO and publishes the detections.

    in : /camera/camera/color/image_raw   sensor_msgs/Image   (same name as realsense-ros)
    out: /cogidrone/detections            vision_msgs/Detection2DArray
         /cogidrone/detections/image      sensor_msgs/Image   (boxes drawn, for rqt_image_view)
"""

import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image
from ultralytics import YOLO
from vision_msgs.msg import Detection2D, Detection2DArray, ObjectHypothesisWithPose


def image_to_bgr(msg: Image) -> np.ndarray:
    """Convert an rgb8/bgr8 ROS image to an OpenCV-style BGR array (no cv_bridge needed)."""
    if msg.encoding not in ("rgb8", "bgr8"):
        raise ValueError(
            f"Unsupported image encoding '{msg.encoding}', expected rgb8 or bgr8"
        )
    rows = np.frombuffer(msg.data, dtype=np.uint8).reshape(msg.height, msg.step)
    img = rows[:, : msg.width * 3].reshape(msg.height, msg.width, 3)
    return img[:, :, ::-1].copy() if msg.encoding == "rgb8" else img


def bgr_to_image(img: np.ndarray, header) -> Image:
    msg = Image()
    msg.header = header
    msg.height, msg.width = img.shape[:2]
    msg.encoding = "bgr8"
    msg.step = msg.width * 3
    msg.data = np.ascontiguousarray(img).tobytes()
    return msg


class Detector(Node):
    def __init__(self):
        super().__init__("detector")
        self.declare_parameter("image_topic", "/camera/camera/color/image_raw")
        self.declare_parameter("model", "/opt/models/yolo11n.pt")
        self.declare_parameter("confidence", 0.4)
        self.declare_parameter("device", "cpu")
        self.declare_parameter("publish_annotated", True)

        model_path = self.get_parameter("model").value
        self.model = YOLO(model_path)
        self.confidence = self.get_parameter("confidence").value
        self.device = self.get_parameter("device").value
        self.publish_annotated = self.get_parameter("publish_annotated").value

        self.det_pub = self.create_publisher(
            Detection2DArray, "/cogidrone/detections", 10
        )
        self.img_pub = self.create_publisher(
            Image, "/cogidrone/detections/image", qos_profile_sensor_data
        )
        # Sensor QoS with depth 1: if YOLO is slower than the camera, old frames are dropped.
        self.create_subscription(
            Image,
            self.get_parameter("image_topic").value,
            self.on_image,
            qos_profile_sensor_data,
        )
        self.get_logger().info(
            f"YOLO '{model_path}' on {self.device}, waiting for images..."
        )

    def on_image(self, msg: Image):
        try:
            frame = image_to_bgr(msg)
        except ValueError as e:
            self.get_logger().error(str(e), throttle_duration_sec=5.0)
            return

        result = self.model.predict(
            frame, conf=self.confidence, device=self.device, verbose=False
        )[0]

        out = Detection2DArray()
        out.header = msg.header
        for xywh, cls, score in zip(
            result.boxes.xywh.tolist(),
            result.boxes.cls.tolist(),
            result.boxes.conf.tolist(),
        ):
            det = Detection2D()
            det.header = msg.header
            det.bbox.center.position.x, det.bbox.center.position.y = xywh[0], xywh[1]
            det.bbox.size_x, det.bbox.size_y = xywh[2], xywh[3]
            hyp = ObjectHypothesisWithPose()
            hyp.hypothesis.class_id = result.names[int(cls)]
            hyp.hypothesis.score = float(score)
            det.results.append(hyp)
            out.detections.append(det)
        self.det_pub.publish(out)

        if self.publish_annotated:
            self.img_pub.publish(bgr_to_image(result.plot(), msg.header))


def main():
    rclpy.init()
    node = Detector()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()
