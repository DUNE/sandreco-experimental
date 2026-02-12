CL_KERNEL(void invert_matrix(__global const float* matrix,  __global float* inverted_matrix)
 {
  const int i = get_global_id(0);
  const int j = get_global_id(1);
  const int k = get_global_id(2);
  const int isize = get_global_size(0);
  const int jsize = get_global_size(1);
  const int ksize = get_global_size(2);
  
  const int v_idx = (i * jsize + j) * ksize + k;  //voxel idx
  if (matrix[v_idx] != 0) {
    inverted_matrix[v_idx] = 1 / matrix[v_idx];
  }
  else {
    inverted_matrix[v_idx] = 0.f;
  }
 })