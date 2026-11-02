"""Training script for merged Search & Rescue YOLO model on IDUN HPC."""

from __future__ import annotations

import os
import torch
import wandb
from ultralytics import YOLO, settings

# 1. Enable Ultralytics W&B integration
settings.update({"wandb": True})

def main():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print("Bruker enhet:", device)
    print(f"Weights & Biases SDK: {wandb.__version__}")

    # 2. Explicitly initialize W&B run
    run = wandb.init(
        project="CogiDroneTraining",
        entity="muh-muwaffaq-cogito",
        name="yolo_sr_final_idun",
        job_type="train",
        reinit=True
    )

    # 3. Load model 
    model = YOLO("models/yolo26n.pt")

    # 4. Train model optimized for IDUN cluster GPUs
    results = model.train(
        data="./python/dataset/sr_final/data.yaml",
        epochs=150,             # Full convergence run
        imgsz=1280,             # Handles your 1080p drone shots perfectly
        batch=16,               # IDUN GPUs (A100/H100/V100) can handle much larger batches at 1280px
        cache=True,             # Loads dataset into system RAM to eliminate disk I/O bottlenecks
        workers=8,              # Multi-worker data loading
        multi_scale=True,       # Crucial for mixing 640px (S&R 1) and 1080p (S&R 2) data
        mosaic=1.0,             
        mixup=0.15,             
        project="CogiDroneTraining",
        name="yolo_sr_final_idun",
        device=device,
        lr0=0.01,               
        plots=True              
    )

    # 5. Validation on val set
    metrics = model.val()
    print("===== Valideringsresultater =====")
    print(f"Nøyaktighet (mAP50-95): {metrics.box.map * 100:.1f}%")
    print(f"Nøyaktighet (mAP50):    {metrics.box.map50 * 100:.1f}%")
    print(f"Precision:              {metrics.box.mp * 100:.1f}%")
    print(f"Recall:                 {metrics.box.mr * 100:.1f}%")

    # 6. Validation on test set
    test_metrics = model.val(split="test")
    print("Test mAP50-95:", test_metrics.box.map)
    print("Test mAP50:", test_metrics.box.map50)

    # 7. Explicitly end W&B logging session
    wandb.finish()

if __name__ == '__main__':
    main()