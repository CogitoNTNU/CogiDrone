import os
import torch
import wandb
from ultralytics import YOLO, settings

# 1. Enable Ultralytics W&B integration
settings.update({"wandb": True})

def main():
    if torch.cuda.is_available():
        device = torch.device("cuda")
    elif torch.backends.mps.is_available():
        device = torch.device("mps")
    else:
        device = torch.device("cpu")

    print("Bruker:", device)
    print(f"Weights & Biases SDK: {wandb.__version__}")

    # 2. Explicitly initialize W&B run
    run = wandb.init(
        project="CogiDroneTraining",
        entity="muh-muwaffaq-cogito",
        name="yolo26n",
        job_type="train",
        reinit=True
    )

    # 3. Load model
    model = YOLO("models/yolo26n.pt")

    # 4. Train model
    results = model.train(
        data="./python/dataset/search-and-rescue/data.yaml",
        epochs=10,
        imgsz=640,
        project="CogiDroneTraining",
        name="yolo26n",
        device=device,
        lr0=0.02, #learning rate
        plots=True  # Ensures loss and metric plots are generated for W&B upload
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