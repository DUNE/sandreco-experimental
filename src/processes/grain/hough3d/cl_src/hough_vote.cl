CL_KERNEL(void hough_vote(__global const float4* points, __global const float4* versors, __global uint* voting_array, const float xy_step, const int n_xy_bins)
{
    const int p_id = get_global_id(0);
    const int v_id = get_global_id(1);

    const float4 p = points[p_id];
    const float4 v = versors[v_id];

    // Since we use the upper hemishpere, v.z > 0 always
    const float inv = 1.0f / (1.0f + v.z);

    const float3 t_x = (float3)(
        1.0f - v.x * v.x * inv,
       -v.x * v.y * inv,
       -v.x
    );

    const float3 t_y = (float3)(
       -v.x * v.y * inv,
        1.0f - v.y * v.y * inv,
       -v.y
    );

    const float y_prime = dot(p.xyz, t_y.xyz);
    const float x_prime = dot(p.xyz, t_x.xyz);

    const int x_index =  n_xy_bins / 2 + (int)(x_prime / xy_step);

    const int y_index = n_xy_bins / 2 + (int)(y_prime / xy_step);

    if (x_index < 0 || x_index >= n_xy_bins || y_index < 0 || y_index >= n_xy_bins) {
        return;
    }

    const int vote_index = v_id * n_xy_bins * n_xy_bins + x_index * n_xy_bins + y_index;

    // Casting explicitly to avoid ambiguity when compiling for NVIDIA GPU
    atomic_fetch_add((__global uint*)&voting_array[vote_index], (uint)1); 
})