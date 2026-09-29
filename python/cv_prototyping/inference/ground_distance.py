"""Avstand til personer via bakkeplan-geometri (kamerahøyde + pitch/roll).

Idé: bunnen av personens bounding box er der føttene treffer bakken. Vi sender en
stråle fra kameraet gjennom det pikselet, og finner hvor strålen treffer bakken.
Da trenger vi bare å vite hvor høyt kameraet er og hvordan det er vinklet, ikke
hvor mange piksler personen er. Det gjør metoden robust på lang avstand og for
personer som sitter/ligger.

Antakelser:
  - Flatt terreng mellom drone og person.
  - Kjent høyde over bakken (AGL) og kjent vinkel på kameraet.

Uten drone kan dette testes med webkamera: sett kameraet på et stativ, mål høyden
og vinkelen (f.eks. med vater-app på mobilen), og fyll inn i TEST_CAMERA_*.
Taster i vinduet: w/s = pitch +/- 0.5°, e/d = høyde +/- 5 cm, q = avslutt.

Koordinatsystem (samme som OpenCV): x til høyre, y nedover, z framover.
"""

import math
from dataclasses import dataclass

import cv2
import numpy as np
from ultralytics import YOLO

from camera import open_webcam, close_webcam

MODEL_WEIGHTS = "yolo26n.pt"
PERSON_CLASS_ID = 0
IMG_SIZE = 1280  # Ikke skaler ned til 640, små personer forsvinner da
CONF_THRESHOLD = 0.25

AVERAGE_HUMAN_HEIGHT_M = 1.70  # Kun brukt til sammenligning med kroppshøyde-metoden

# Under denne vinkelen (grader under horisonten) blir estimatet svært unøyaktig,
# fordi en liten pikselfeil gir stor avstandsfeil nær horisonten.
MIN_DEPRESSION_DEG = 2.0

# Verdier for testing med webkamera på stativ (overstyres av dronen senere)
TEST_CAMERA_HEIGHT_M = 1.5
TEST_CAMERA_PITCH_DEG = 10.0  # Positiv = kamera vinklet nedover
TEST_CAMERA_ROLL_DEG = 0.0


# ---------------------------------------------------------
# Kamera-intrinsics
# ---------------------------------------------------------
@dataclass
class CameraIntrinsics:
    fx: float
    fy: float
    cx: float
    cy: float
    dist_coeffs: np.ndarray | None = None  # Linseforvrengning fra kalibrering

    @property
    def matrix(self) -> np.ndarray:
        return np.array([[self.fx, 0, self.cx], [0, self.fy, self.cy], [0, 0, 1]], dtype=np.float64)


def load_intrinsics(width: int, height: int) -> CameraIntrinsics:
    """Returner kameraets intrinsics.

    TODO: Kalibrer dronekameraet med sjakkbrett (cv2.calibrateCamera) og last inn
    fx, fy, cx, cy og dist_coeffs herfra, f.eks. fra en .yaml/.npz-fil. Kalibrer
    med samme oppløsning som brukes i flukt (1280x720). En feil i fx/fy gir direkte
    feil i vinkelen til hvert piksel, og dermed i avstanden.
    """
    focal = width * 0.8  # Grovt anslag, samme som i depth_head.py. Byttes ut med kalibrering.
    return CameraIntrinsics(fx=focal, fy=focal, cx=width / 2.0, cy=height / 2.0)


# ---------------------------------------------------------
# Drone-/kameratilstand
# ---------------------------------------------------------
@dataclass
class CameraPose:
    height_m: float   # Kameraets høyde over bakken (AGL), ikke over havet
    pitch_deg: float  # Vinkel under horisonten, positiv = ned
    roll_deg: float   # Rotasjon rundt kameraets optiske akse
    timestamp: float = 0.0


def get_camera_pose(test_pose: CameraPose) -> CameraPose:
    """Hent kameraets høyde og orientering for bildet som prosesseres nå.

    Foreløpig returneres testverdiene (webkamera på stativ).

    TODO (drone):
      - Høyde: bruk høyde over BAKKEN. Barometer/GPS gir høyde relativt til
        takeoff eller havnivå, som blir feil i skrånende terreng. En nedoverrettet
        lidar/rangefinder er best (MAVLink: DISTANCE_SENSOR). Fallback:
        GLOBAL_POSITION_INT.relative_alt hvis bakken er flat.
      - Pitch/roll: hvis kameraet sitter på en stabilisert gimbal, bruk gimbalens
        absolutte vinkler (MAVLink: GIMBAL_DEVICE_ATTITUDE_STATUS / MOUNT_STATUS).
        Hvis kameraet er fastmontert: dronens pitch/roll (ATTITUDE) + monteringsvinkelen.
      - Legg på offset mellom autopilotens høydereferanse og kameraets faktiske posisjon.
      - Synkronisering: bruk tilstanden fra samme tidspunkt som bildet ble tatt,
        ikke den siste mottatte. Ved raske manøvrer kan 50-100 ms forsinkelse gi
        flere graders feil i pitch. Lagre en buffer med timestampede målinger og
        interpoler til bildets timestamp.
      - Sjekk fortegn på pitch/roll mot konvensjonen i dette skriptet (se
        rotation_camera_to_level). Enkel test: tegnet horisont skal ligge på den
        ekte horisonten.
    """
    return test_pose


# ---------------------------------------------------------
# Geometri
# ---------------------------------------------------------
def rotation_camera_to_level(pitch_deg: float, roll_deg: float) -> np.ndarray:
    """Rotasjonsmatrise fra kamerakoordinater til et horisontalt (nivellert) system.

    I det nivellerte systemet peker y rett ned mot bakken og z horisontalt framover.
    Yaw påvirker ikke avstanden, så den er utelatt.
    """
    p = math.radians(pitch_deg)
    r = math.radians(roll_deg)
    # Pitch: kameraets z-akse vippes ned mot +y
    r_pitch = np.array([
        [1, 0, 0],
        [0, math.cos(p), math.sin(p)],
        [0, -math.sin(p), math.cos(p)],
    ])
    # Roll: rotasjon rundt optisk akse (z)
    r_roll = np.array([
        [math.cos(r), -math.sin(r), 0],
        [math.sin(r), math.cos(r), 0],
        [0, 0, 1],
    ])
    return r_pitch @ r_roll


def pixel_to_ray(u: float, v: float, intr: CameraIntrinsics) -> np.ndarray:
    """Retningsvektor i kamerakoordinater for et piksel, korrigert for linseforvrengning."""
    if intr.dist_coeffs is not None:
        pts = np.array([[[u, v]]], dtype=np.float64)
        x, y = cv2.undistortPoints(pts, intr.matrix, intr.dist_coeffs)[0, 0]
        return np.array([x, y, 1.0])
    return np.array([(u - intr.cx) / intr.fx, (v - intr.cy) / intr.fy, 1.0])


@dataclass
class GroundEstimate:
    ground_dist_m: float      # Horisontal avstand langs bakken
    slant_dist_m: float       # Rett linje fra kamera til punktet (siktlinje)
    lateral_m: float          # Sideveis forskyvning (+ = til høyre)
    depression_deg: float     # Vinkel under horisonten til punktet
    error_per_px_m: float     # Hvor mye avstanden endres ved 1 px feil i bunnen av boksen


def _intersect_ground(u: float, v: float, intr: CameraIntrinsics, pose: CameraPose) -> np.ndarray | None:
    ray = rotation_camera_to_level(pose.pitch_deg, pose.roll_deg) @ pixel_to_ray(u, v, intr)
    if ray[1] <= 1e-9:
        return None  # Strålen peker over horisonten og treffer aldri bakken
    t = pose.height_m / ray[1]
    return ray * t


def estimate_ground_distance(u: float, v: float, intr: CameraIntrinsics, pose: CameraPose) -> GroundEstimate | None:
    """Finn hvor strålen gjennom piksel (u, v) treffer bakken."""
    point = _intersect_ground(u, v, intr, pose)
    if point is None:
        return None

    ground_dist = float(math.hypot(point[0], point[2]))
    slant_dist = float(np.linalg.norm(point))
    depression = math.degrees(math.atan2(point[1], ground_dist))

    # Følsomhet: flytt punktet 1 px ned og se hvor mye avstanden endrer seg
    point_below = _intersect_ground(u, v + 1, intr, pose)
    error_per_px = abs(ground_dist - math.hypot(point_below[0], point_below[2])) if point_below is not None else float("inf")

    return GroundEstimate(ground_dist, slant_dist, float(point[0]), depression, error_per_px)


def horizon_line(width: int, intr: CameraIntrinsics, pose: CameraPose) -> tuple[tuple[int, int], tuple[int, int]] | None:
    """To punkter på horisontlinjen i bildet. Nyttig for å sjekke pitch/roll visuelt."""
    m = rotation_camera_to_level(pose.pitch_deg, pose.roll_deg)[1]
    if abs(m[1]) < 1e-9:
        return None

    def v_at(u: float) -> float:
        return intr.cy - intr.fy * (m[2] + m[0] * (u - intr.cx) / intr.fx) / m[1]

    return (0, int(v_at(0))), (width, int(v_at(width)))


# ---------------------------------------------------------
# Live-løkke
# ---------------------------------------------------------
def main() -> None:
    cap = open_webcam(width=1280, height=720, camera_index=1)
    model = YOLO(MODEL_WEIGHTS)

    test_pose = CameraPose(TEST_CAMERA_HEIGHT_M, TEST_CAMERA_PITCH_DEG, TEST_CAMERA_ROLL_DEG)
    intr = None

    while True:
        success, img = cap.read()
        if not success:
            continue

        h, w, _ = img.shape
        if intr is None:
            intr = load_intrinsics(w, h)

        # TODO (drone): hent timestamp for bildet her og send det inn, se get_camera_pose
        pose = get_camera_pose(test_pose)

        horizon = horizon_line(w, intr, pose)
        if horizon is not None:
            cv2.line(img, horizon[0], horizon[1], (0, 255, 255), 1)

        results = model(img, imgsz=IMG_SIZE, conf=CONF_THRESHOLD, classes=[PERSON_CLASS_ID], verbose=False)

        for box in results[0].boxes:
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            foot_u, foot_v = (x1 + x2) / 2.0, float(y2)

            body_dist = (AVERAGE_HUMAN_HEIGHT_M * intr.fy) / max(y2 - y1, 1)
            est = estimate_ground_distance(foot_u, foot_v, intr, pose)

            # Føttene er kuttet av bildekanten, bunnen av boksen er ikke bakkepunktet
            feet_cut = y2 >= h - 2

            if est is None:
                ground_txt, color = "Ground: over horisont", (0, 0, 255)
            elif feet_cut or est.depression_deg < MIN_DEPRESSION_DEG:
                ground_txt, color = f"Ground: ~{est.ground_dist_m:.1f}m (upålitelig)", (0, 165, 255)
            else:
                ground_txt, color = f"Ground: {est.ground_dist_m:.1f}m ±{est.error_per_px_m:.2f}/px", (0, 255, 0)

            # TODO: kombiner ground-, kropps- og hodeestimat (se depth_head.py) til ett
            # tall, vektet etter usikkerhet, og glatt over tid med Kalman-filter per person.
            label = f"{ground_txt} | Body: {body_dist:.1f}m"

            cv2.rectangle(img, (x1, y1), (x2, y2), color, 2)
            cv2.circle(img, (int(foot_u), int(foot_v)), 4, color, -1)
            cv2.putText(img, label, (x1, max(y1 - 10, 20)), cv2.FONT_HERSHEY_SIMPLEX, 0.55, color, 2)

        status = f"Hoyde: {test_pose.height_m:.2f}m  Pitch: {test_pose.pitch_deg:.1f}  Roll: {test_pose.roll_deg:.1f}"
        cv2.putText(img, status, (10, h - 15), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        cv2.imshow("Bakkeplan-avstand", img)
        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break
        elif key == ord("w"):
            test_pose.pitch_deg += 0.5
        elif key == ord("s"):
            test_pose.pitch_deg -= 0.5
        elif key == ord("e"):
            test_pose.height_m += 0.05
        elif key == ord("d"):
            test_pose.height_m = max(0.05, test_pose.height_m - 0.05)

    close_webcam(cap)


if __name__ == "__main__":
    main()
