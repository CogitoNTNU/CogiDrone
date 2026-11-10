#!/usr/bin/env bash


# ? This script is used to export YOLO models to ONNX format.

# * Export the models to ONNX format on a pc with ultralytics installed
# Person model
yolo export model=best.pt   format=onnx imgsz=640 nms=False

# Head model
yolo export model=medium.pt format=onnx imgsz=640 nms=False
