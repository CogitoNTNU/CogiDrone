#!/bin/bash
#SBATCH --job-name=yolo_train
#SBATCH --account=studiegrupper-cogito
#SBATCH --time=02:00:00
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --partition=GPUQ
#SBATCH --gpus=1
#SBATCH --output=yolo_%j.out

module purge
module load Python/3.11.5-GCCcore-13.2.0

uv run python python/cv_training/training.py