"""Recreate YOLO split structure and move labels for filtered images."""

from __future__ import annotations

import argparse
import shutil
from dataclasses import dataclass
from pathlib import Path


DEFAULT_SOURCE_DIR = (
    Path(__file__).resolve().parents[1] / "dataset" / "search-and-rescue-2"
)
DEFAULT_FILTERED_DIR = (
    Path(__file__).resolve().parents[1] / "dataset" / "sr_yolo_filtered-2"
)
SPLITS = ("test", "train", "valid")
IMAGE_EXTENSIONS = frozenset(
    {".bmp", ".jpeg", ".jpg", ".png", ".tif", ".tiff", ".webp"}
)


@dataclass(frozen=True)
class MoveSummary:
    """Counts produced by one restructuring run."""

    images_moved: int = 0
    labels_moved: int = 0
    labels_missing: int = 0


def parse_args() -> argparse.Namespace:
    """Parse command-line options with repository-relative defaults."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=DEFAULT_SOURCE_DIR,
        help="Original dataset containing split/images and split/labels.",
    )
    parser.add_argument(
        "--filtered-dir",
        type=Path,
        default=DEFAULT_FILTERED_DIR,
        help="Filtered dataset containing the selected images.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show planned moves without changing any files.",
    )
    return parser.parse_args()


def _move(source: Path, destination: Path, dry_run: bool) -> None:
    """Move one file unless it is already at its destination."""
    if source == destination:
        return
    if destination.exists():
        raise FileExistsError(f"Destination already exists: {destination}")
    if not dry_run:
        shutil.move(str(source), str(destination))


def move_filtered_labels(
    source_dir: Path,
    filtered_dir: Path,
    *,
    dry_run: bool = False,
) -> MoveSummary:
    """Move filtered images and their matching source labels into YOLO folders."""
    if not source_dir.is_dir():
        raise FileNotFoundError(f"Source dataset directory does not exist: {source_dir}")
    if not filtered_dir.is_dir():
        raise FileNotFoundError(
            f"Filtered dataset directory does not exist: {filtered_dir}"
        )

    images_moved = 0
    labels_moved = 0
    labels_missing = 0

    for split in SPLITS:
        source_split = source_dir / split
        filtered_split = filtered_dir / split
        if not source_split.is_dir() or not filtered_split.is_dir():
            continue

        source_labels = source_split / "labels"
        filtered_images = filtered_split / "images"
        filtered_labels = filtered_split / "labels"
        if not source_labels.is_dir():
            raise FileNotFoundError(
                f"Source labels directory does not exist: {source_labels}"
            )
        if not dry_run:
            filtered_images.mkdir(parents=True, exist_ok=True)
            filtered_labels.mkdir(parents=True, exist_ok=True)

        candidates = sorted(
            path
            for path in filtered_split.iterdir()
            if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS
        )
        for image_path in candidates:
            destination_image = filtered_images / image_path.name
            source_label = source_labels / f"{image_path.stem}.txt"
            destination_label = filtered_labels / source_label.name

            _move(image_path, destination_image, dry_run)
            images_moved += 1

            if not source_label.is_file():
                labels_missing += 1
                continue
            _move(source_label, destination_label, dry_run)
            labels_moved += 1

    return MoveSummary(
        images_moved=images_moved,
        labels_moved=labels_moved,
        labels_missing=labels_missing,
    )


def main() -> None:
    args = parse_args()
    summary = move_filtered_labels(
        source_dir=args.source_dir,
        filtered_dir=args.filtered_dir,
        dry_run=args.dry_run,
    )
    action = "Would move" if args.dry_run else "Moved"
    print(
        f"{action} {summary.images_moved} images and {summary.labels_moved} labels; "
        f"{summary.labels_missing} matching labels were not found."
    )


if __name__ == "__main__":
    main()
