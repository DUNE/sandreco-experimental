CL_KERNEL(void multiply_matrices_in_place(__global float* matrix_a, __global const float* matrix_b)
 {
  const int i = get_global_id(0);
  const int j = get_global_id(1);
  const int k = get_global_id(2);
  const int isize = get_global_size(0);
  const int jsize = get_global_size(1);
  const int ksize = get_global_size(2);
  
  const int v_idx = (i * jsize + j) * ksize + k;  //voxel idx
  matrix_a[v_idx] *= matrix_b[v_idx];
 })