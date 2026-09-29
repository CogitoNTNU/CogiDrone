import wandb
from ultralytics import YOLO, settings

import torch

if torch.cuda.is_available():
    device = torch.device("cuda")   # NVIDIA-GPU, f.eks. på Idun
elif torch.backends.mps.is_available():
    device = torch.device("mps")    # Apple-GPU, på Mac
else:
    device = torch.device("cpu")    # fallback

print("Bruker:", device)

print(f"Weights & Biases SDK: {wandb.__version__}")


# Enable Ultralytics' built-in training callbacks (this persists in settings.json).
settings.update({"wandb": True})

def main():
    run = wandb.init(
    # Set the wandb entity where your project will be logged (generally your team name).
    entity="muh-muwaffaq-cogito",
    # Set the wandb project where this run will be logged.
    project="CogiDroneTraining",
    # Track hyperparameters and run metadata.
    config={
        "learning_rate": 0.02,
        "architecture": "CNN",
        "dataset": "CIFAR-100",
        "epochs": 10,
    },
)

    #Load pretrained model
    model = YOLO("models/yolo26n.pt")

    #trains model with data.yaml from personens seen from above
    results = model.train(
        data="./python/dataset/search-and-rescue/data.yaml",
        epochs=10,
        imgsz=640,
        project="CogiDroneTraining",
        name="yolo26n"
    )

    #Validitating with best.pt on val-datasett
    metrics = model.val()
    print("===== Valideringsresultater =====")
    print(f"Nøyaktighet (mAP50-95): {metrics.box.map * 100:.1f}%   <- strengeste mål")
    print(f"Nøyaktighet (mAP50):    {metrics.box.map50 * 100:.1f}%   <- mer lowkey mål")
    print(f"Precision:              {metrics.box.mp * 100:.1f}%   <- hvor mange av deteksjonene var riktige")
    print(f"Recall:                 {metrics.box.mr * 100:.1f}%   <- hvor mange av personene ble faktisk funnet")

    #Validitating with test which it have never seen before
    test_metrics = model.val(split="test")
    print("Test mAP50-95:", test_metrics.box.map)
    print("Test mAP50:", test_metrics.box.map50)

if __name__ == '__main__':
    main()