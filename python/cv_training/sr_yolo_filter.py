"""Filter Search-and-Rescue images with a YOLO person detector.

The source dataset is expected to contain ``test``, ``train``, and ``valid``
directories, each with an ``images`` directory. Images containing at least one
person detection are copied to the output directory together with an annotated
copy showing each detection confidence.
"""

from __future__ import annotations

import argparse
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import cv2
from ultralytics import YOLO



DEFAULT_MODEL = "models/yolo26n.pt"
DEFAULT_CONFIDENCE = 0.25
SPLITS = ("test", "train", "valid")
IMAGE_EXTENSIONS = frozenset({".bmp", ".jpeg", ".jpg", ".png", ".tif", ".tiff", ".webp"})


@dataclass(frozen=True)
class FilterSummary:
    """Counts produced by one filtering run."""

    processed: int = 0
    with_humans: int = 0


def parse_args() -> argparse.Namespace:
    """Parse command-line options while keeping repository-relative defaults."""
    repository_python = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=repository_python / "dataset" / "search-and-rescue-2",
        help="Search-and-Rescue dataset directory.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repository_python / "dataset" / "sr_yolo_filtered-2",
        help="Directory receiving positive and annotated images.",
    )
    parser.add_argument(
        "--model",
        default=DEFAULT_MODEL,
        help="YOLO weights path or Ultralytics model name.",
    )
    parser.add_argument(
        "--confidence",
        type=float,
        default=DEFAULT_CONFIDENCE,
        help="Minimum confidence for a human detection.",
    )
    parser.add_argument(
        "--device",
        default=None,
        help="Inference device, such as 'cpu', '0', or '0,1'.",
    )
    return parser.parse_args()


def image_paths(source_dir: Path) -> Iterable[tuple[str, Path]]:
    """Yield every image in every required dataset split in stable order."""
    for split in SPLITS:
        image_dir = source_dir / split / "images"
        if not image_dir.is_dir():
            raise FileNotFoundError(f"Dataset image directory does not exist: {image_dir}")
        for path in sorted(image_dir.iterdir()):
            if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
                yield split, path


def filter_images(
    source_dir: Path,
    output_dir: Path,
    model_name: str,
    confidence: float,
    device: str | None,
) -> FilterSummary:
    """Run YOLO and copy positive images plus confidence-annotated companions."""
    if not 0.0 <= confidence <= 1.0:
        raise ValueError(f"Confidence must be between 0 and 1, got {confidence}")

    model = YOLO(model_name)
    processed = 0
    with_humans = 0

    for split, image_path in image_paths(source_dir):
        processed += 1
        prediction_kwargs = {
            "source": str(image_path),
            "classes": [0],
            "conf": confidence,
            "verbose": False,
        }
        if device is not None:
            prediction_kwargs["device"] = device

        results = model.predict(**prediction_kwargs)
        if not results:
            raise RuntimeError(f"YOLO returned no result for {image_path}")
        result = results[0]
        if result.boxes is None or len(result.boxes) == 0:
            continue

        split_output_dir = output_dir / split
        split_output_dir.mkdir(parents=True, exist_ok=True)
        copied_path = split_output_dir / image_path.name
        annotated_path = split_output_dir / (
            f"{image_path.stem}_human_detections{image_path.suffix}"
        )

        shutil.copy2(image_path, copied_path)
        annotated = result.plot(labels=True, conf=True, boxes=True)
        if not cv2.imwrite(str(annotated_path), annotated):
            raise OSError(f"Could not write annotated image: {annotated_path}")
        with_humans += 1

    return FilterSummary(processed=processed, with_humans=with_humans)


def main() -> None:
    args = parse_args()
    summary = filter_images(
        source_dir=args.source_dir,
        output_dir=args.output_dir,
        model_name=args.model,
        confidence=args.confidence,
        device=args.device,
    )
    print(
        f"Processed {summary.processed} images; "
        f"copied {summary.with_humans} images containing humans to {args.output_dir}."
    )


if __name__ == "__main__":
    main()