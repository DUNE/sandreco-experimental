import numpy as np
import json

voxel_size = 0

def configure(jstr):
  cfg = json.loads(jstr)
  global voxel_size
  voxel_size = cfg["voxel_size"]
  print(f"Voxel size: {voxel_size} mm")

def run(fiducial, transforms, images):
  print(f"Voxel size: {voxel_size} mm")
  print(f"The shape of fiducial is {fiducial.shape}")
  print(f"There are transforms for {len(transforms)} cameras:")
  for name, xform in transforms.items():
    print(f"Camera {name} xform =\n{xform}")
  print(f"Processing {len(images)} images")
  for name, image in images.items():
    print(f"Processing image for camera {name}, which is of shape {image.shape}")
  ret = np.zeros(fiducial.shape, dtype=float)
  return ret
