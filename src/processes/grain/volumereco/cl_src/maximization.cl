CL_KERNEL(void maximization(__global const float* system_matrix, __global const float* inverted_sensitivity_matrix, const int n_sensors, const float pde,
                            __global const float* expectation_input, __global const float* image, __global float* maximization_result)
 {
  const int i = get_global_id(0);
  const int j = get_global_id(1);
  const int k = get_global_id(2);
  const int isize = get_global_size(0);
  const int jsize = get_global_size(1);
  const int ksize = get_global_size(2);
  
  const int v_idx = (i * jsize + j) * ksize + k;  //voxel idx
  double vscore = 0.0;
  
  for (int s_idx = 0; s_idx != n_sensors; ++s_idx) {  //sensor loop
    const float npe = image[s_idx];
    const float estep = expectation_input[s_idx];
    const int sm_idx = i * jsize * ksize * n_sensors + j * ksize * n_sensors + k * n_sensors + s_idx; //sysmat idx
    const float w = pde * system_matrix[sm_idx] * inverted_sensitivity_matrix[v_idx];    
    vscore += (npe * w * estep);    
   }   
  // Add to result comung from other cameras (same GPU queue)
  maximization_result[v_idx] += (float)vscore ;
 })
