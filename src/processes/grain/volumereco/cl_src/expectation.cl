CL_KERNEL(void expectation(__global const float* system_matrix, __global const float* inverted_sensitivity_matrix, const int n_voxels, const float pde, __global const float* previous_amplitude,  __global float* expectation_result) {
    //sensor index
    const int s_idx = get_global_id(0);
    const int s_size = get_global_size(0);
    
    float e_res = 0.f;

    for (uint vox_idx = 0; vox_idx < n_voxels; ++vox_idx)  {
          const int sm_idx = vox_idx * s_size + s_idx;
          e_res += pde * system_matrix[sm_idx] * inverted_sensitivity_matrix[vox_idx] * previous_amplitude[vox_idx];
        }
    expectation_result[s_idx] = (e_res <= 0.f) ? 0.f : 1.f / e_res;       
})
