CL_KERNEL(void expectation(__global const float* sys_mat, __global const float* sensitivity_inv,  uint3 grid, __global const float* previous_iteration_score,  __global float* expectation_result) {
    //sensor index
    const int s_idx = get_global_id(0);
    const int s_size = get_global_size(0);
    
    float e_res = 0.f;
    int sm_idx;
    float pde = 1.f;

    for (uint vox_idx = 0; vox_idx < grid.x * grid.y * grid.z; ++vox_idx)  {
          sm_idx = vox_idx * s_size + s_idx;
          e_res += pde * sys_mat[sm_idx] * sensitivity_inv[vox_idx] * previous_iteration_score[vox_idx];
        }
    expectation_result[s_idx] = (e_res <= 0.f) ? 0.f : 1.f / e_res;       

})
