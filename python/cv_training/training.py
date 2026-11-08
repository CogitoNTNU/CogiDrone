from __future__ import annotations

import os
import torch
import wandb
from ultralytics import YOLO, settings

settings.update({"wandb": True})

def main():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print("Bruker enhet:", device)
    print(f"Weights & Biases SDK: {wandb.__version__}")

    run = wandb.init(
        project="CogiDroneTraining",
        entity="muh-muwaffaq-cogito",
        name="yolo_sr_final_idun",
        job_type="train",
        reinit=True
    )

    model = YOLO("models/yolo26n.pt")

    results = model.train(
        data="./python/dataset/sr_final/data.yaml",
        epochs=150,
        imgsz=1280,
        batch=4, 
        workers=4,
        multi_scale=False,
        mosaic=1.0,             
        mixup=0.15,             
        project="CogiDroneTraining",
        name="yolo_sr_final_last",
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

    wandb.finish()

if __name__ == '__main__':
    main()