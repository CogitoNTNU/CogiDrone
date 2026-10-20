> [!CAUTION]
> **THIS NOTE IS OUTDATED!** The sensor list below is the original idea. See "Today" for what the code does.

### Perception 
- IMU: acceleration + rotation
- Cameras: visual information
- Range/depth sensor: obstacles/distances
- GPS/GNSS (when available)
- Barometer: altitude
- Controller input: received manual input from physical controller 

**Today:** `Perception` subscribes to `/camera/image` (the launch file remaps it to
`/camera/camera/color/image_raw`, the realsense-ros name) and will publish
`/perception/detections`. The `Person`, `Head` and `DepthAnything` models are stubs, so nothing
is published yet. In the sim, `sim_cpp.launch.py yolo:=true` feeds the Python YOLO detector's
boxes to Navigation instead. See [docs/architecture.md](../../docs/architecture.md).
