CL_KERNEL(void maximization(__global const float* expectation_step_res, __global const float* hits, __global const float* sys_mat, 
                                __global const float* sensitivity_inv, const int sensor_pixels, __global const char* mask, __global float* voxel_score)
 {
  const int i = get_global_id(0);
  const int j = get_global_id(1);
  const int k = get_global_id(2);
  const int isize = get_global_size(0);
  const int jsize = get_global_size(1);
  const int ksize = get_global_size(2);
  
  const int v_idx = (i * jsize + j) * ksize + k;  //voxel idx
  const float pde = 1.f;

  //if (!mask[v_idx])
  //  return;
    
  double vscore = 0.0;
  
  for (int s_idx = 0; s_idx != sensor_pixels; ++s_idx) {  //sensor loop
    float npe = hits[s_idx];
    float estep = expectation_step_res[s_idx];
    int sm_idx = i * jsize * ksize * sensor_pixels + j * ksize * sensor_pixels + k * sensor_pixels + s_idx; //sysmat idx
    float w = pde * sys_mat[sm_idx] * sensitivity_inv[v_idx];    
    vscore += (npe * w * estep);    

   }   

  voxel_score[v_idx] = (float)vscore ;

 })
