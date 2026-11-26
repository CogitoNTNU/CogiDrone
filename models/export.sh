#!/usr/bin/env bash


# ? This script is used to export YOLO models to ONNX format.

# * Export the models to ONNX format on a pc with ultralytics installed
# Person model
yolo export model=best.pt   format=onnx imgsz=640 nms=False

# Head model
yolo export model=medium.pt format=onnx imgsz=640 nms=False


# ! VERIFY THE OUTPUT LAYOUT AFTER EXPORTING. 
# ? The C++ decoder is selected by layout, and a 
# ? mismatch fails at runtime with a logged error.
# ?
# ?   best.onnx    -> [1, 300, 6]   END_TO_END  (YOLO26, NMS-free)
# ?   medium.onnx  -> [1, 5, 8400]  ANCHORS     (4 box + 1 class, NMS required)
# ?
# ? Check with:
# ?   python -c "MODEL_FILE='best'; import onnx; m=onnx.load(f'models/{MODEL_FILE}' + ('.onnx' if '.onnx' not in MODEL_FILE else '')); print([d.dim_value for d in m.graph.output[0].type.tensor_type.shape.dim])"
# ? 
# ! Remember to set the `MODEL_FILE` variable within the Python snipped to the desired model!