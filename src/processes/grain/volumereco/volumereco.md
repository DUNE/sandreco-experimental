# Volumereco

Iterative GPU-accelerated algorithm based on Maximum Likelihood Expectation Maximization for GRAIN (masks). It tries to reconstruct the 3D photon source distribution by an iterative search for the solution that is most
consistent with the detector data.

## Input

- camera images, produced by `spill_slicer`
- voxel weights, produced by `mask_weights_computation`

## Output

- 3D voxelized photon source distribution

## Steps

Each iteration of the algorithm goes through two steps: 
- `expectation`, which determines the expected projection from the current estimate of the source distribution
- `maximization`, which computes the relative difference between the estimated and measured projections to move the current source distribution to be closer to the solution

## Computing resources

Thanks to the OpenCL framework, this process can run both on CPU(s) or GPU(s), nevertheless a GPU is mandatory for a full-scale reconstruction.
If multiple GPUs are available, the computations are distributed evenly. 

The following table summarize the (total) VRAM required for this process, considering the full GRAIN geometry, as a function of the voxel size.

| voxel size (mm) | VRAM required (GB) |
| ------ | ------ |
|   10     |    107    |
|   12     |    97    |
|   14     |    55    |
|   16     |    40    |
|   18     |    29    |
|   20     |    20    |
