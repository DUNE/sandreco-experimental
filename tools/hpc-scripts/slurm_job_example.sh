#!/bin/bash
#
#SBATCH --job-name=job_name
#SBATCH --output=<path/to/production/folder>/job.out
#SBATCH --error=<path/to/production/folder>/job.err
#SBATCH --partition=bare-metal-nodes
#SBATCH --gres=gpu:1


# if running ufw with software already installed in production image sand-prod-nv
apptainer run --nv --no-home --bind <production/directory>:/home/sand  /storage-hpc/ntosi/SAND-LAr-BIN/sandreco-docker/sand-prod-nv_latest.sif ufwrun /home/sand/<config/file/in/production/dir>
:1

# if you need to pull and install sandreco-experimental from a specific branch:
apptainer run --nv --fakeroot --no-home --writable-tmpfs --env-file <path/to/txt/containing/sandreco GIT_TOKEN>  --bind <production/directory>:/home/sand  /storage-hpc/ntosi/SAND-LAr-BIN/sandreco-docker/sand-prod-nv_latest.sif /home/sand/container_pull_test_example.sh

