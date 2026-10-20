"""Velger hvilken person dronen skal fly mot: nærmeste blant sikre deteksjoner."""

from __future__ import annotations

from dataclasses import dataclass

MIN_CONFIDENCE = 0.4        # Deteksjoner under dette ignoreres helt
SWITCH_MARGIN = 0.8         # Ny kandidat må være minst 20 % nærmere for å bytte target
MAX_MISSED_FRAMES = 10      # Antall frames vi venter på at target dukker opp igjen
MIN_IOU_SAME_TARGET = 0.3   # Overlapp mellom frames som regnes som samme person
MIN_SAFE_DISTANCE_M = 2.0   # Under denne avstanden skal dronen stoppe


@dataclass
class Detection:
    bbox: tuple[int, int, int, int]  # (x1, y1, x2, y2) i piksler
    conf: float
    distance: float                  # Meter. Hodeavstand hvis tilgjengelig, ellers kropp
    track_id: int | None = None      # Fra model.track(...), hvis brukt


def iou(a, b):
    """Intersection over union mellom to bokser (x1, y1, x2, y2)."""
    ix1, iy1 = max(a[0], b[0]), max(a[1], b[1])
    ix2, iy2 = min(a[2], b[2]), min(a[3], b[3])
    inter = max(0, ix2 - ix1) * max(0, iy2 - iy1)
    area_a = (a[2] - a[0]) * (a[3] - a[1])
    area_b = (b[2] - b[0]) * (b[3] - b[1])
    union = area_a + area_b - inter
    return inter / union if union > 0 else 0.0


def is_too_close(target):
    return target is not None and target.distance < MIN_SAFE_DISTANCE_M


class ClosestTargeter:
    """Holder på valgt target mellom frames, så dronen ikke flakker mellom personer.

    Kall update() én gang per frame med alle person-deteksjoner.
    Returnerer target å fly mot, eller None (dronen bør holde posisjon).
    """

    def __init__(self):
        self.current = None
        self.missed_frames = 0

    def _find_current(self, candidates):
        """Finner forrige target blant nye deteksjoner, via track_id eller overlapp."""
        if self.current is None:
            return None
        if self.current.track_id is not None:
            for d in candidates:
                if d.track_id == self.current.track_id:
                    return d
            return None
        best = max(candidates, key=lambda d: iou(d.bbox, self.current.bbox), default=None)
        if best is not None and iou(best.bbox, self.current.bbox) >= MIN_IOU_SAME_TARGET:
            return best
        return None

    def update(self, detections):
        candidates = [d for d in detections if d.conf >= MIN_CONFIDENCE]
        closest = min(candidates, key=lambda d: d.distance, default=None)
        current = self._find_current(candidates)

        if current is not None:
            self.missed_frames = 0
            # Bytt bare hvis en annen person er klart nærmere
            if closest is not current and closest.distance < current.distance * SWITCH_MARGIN:
                current = closest
            self.current = current
            return self.current

        # Target forsvant: vent noen frames før vi velger en ny
        if self.current is not None and self.missed_frames < MAX_MISSED_FRAMES:
            self.missed_frames += 1
            return None

        self.current = closest
        self.missed_frames = 0
        return self.current
