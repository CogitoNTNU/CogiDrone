"""Delte hjelpefunksjoner for cv_prototyping-skriptene (webcam + YOLO-klassenavn)."""

import cv2
import numpy as np
import pyrealsense2 as rs


def is_realsense_available() -> bool:
    """Sjekker om en RealSense-enhet er tilkoblet."""
    try:
        ctx = rs.context()
        devices = ctx.query_devices()
        return len(devices) > 0
    except Exception:
        return False


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


def run_camera_stream():
    """Kjører kamerastrømmen. Bruker RealSense hvis tilgjengelig, ellers standard webcam."""
    if is_realsense_available():
        print("RealSense-kamera oppdaget. Starter RealSense-pipeline...")
        
        # 1. Initialize RealSense pipeline
        pipeline = rs.pipeline()
        config = rs.config()

        # 2. Configure the streams (Depth and RGB)
        config.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
        config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)

        # 3. Start streaming
        pipeline.start(config)

        try:
            while True:
                # Wait for a coherent pair of frames: depth and color
                frames = pipeline.wait_for_frames()
                depth_frame = frames.get_depth_frame()
                color_frame = frames.get_color_frame()

                if not depth_frame or not color_frame:
                    continue

                # Convert images to numpy arrays
                depth_image = np.asanyarray(depth_frame.get_data())
                color_image = np.asanyarray(color_frame.get_data())

                # Apply colormap on depth image (image must be converted to 8-bit per pixel first)
                depth_colormap = cv2.applyColorMap(
                    cv2.convertScaleAbs(depth_image, alpha=0.03), cv2.COLORMAP_JET
                )

                # Stack both images horizontally
                images = np.hstack((color_image, depth_colormap))

                # Show images
                cv2.imshow('RealSense | Color (Left) & Depth (Right)', images)

                # Press 'q' or 'ESC' to exit
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
        finally:
            # Stop streaming
            pipeline.stop()
            cv2.destroyAllWindows()
            
    else:
        print("Ingen RealSense-enhet funnet. Faller tilbake til standard webcam...")
        cap = open_webcam()
        
        try:
            while True:
                ret, frame = cap.read()
                if not ret:
                    print("Kunne ikke hente bilde fra webcam.")
                    break
                    
                cv2.imshow('Webcam Fallback', frame)
                
                # Press 'q' or 'ESC' to exit
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
        finally:
            close_webcam(cap)


if __name__ == "__main__":
    run_camera_stream()