import random
import wandb

# Start en ny wandb-kjøring (run) som sporer dette skriptet.
run = wandb.init(
    entity="muh-muwaffaq-cogito",
    project="CogiDroneTraining",
    config={
        "learning_rate": 0.02,
        "architecture": "CNN",
        "dataset": "CIFAR-100",
        "epochs": 10,
    },
)

# Simuler trening.
epochs = 10
offset = random.random() / 5
for epoch in range(2, epochs):
    acc = 1 - 2**-epoch - random.random() / epoch - offset
    loss = 2**-epoch + random.random() / epoch + offset

    # Logg metrikker til wandb.
    run.log({"acc": acc, "loss": loss})

# Avslutt kjøringen og last opp resten av dataene.
run.finish()