CL_KERNEL(void hough_distance(__global const float4* points, const float3 line_versor, const float3 line_point, __global float* distances)
{
    const int p_id = get_global_id(0);

    const float numerator = length(cross((points[p_id].xyz - line_point), line_versor));
    const float denominator = length(line_versor);

    distances[p_id] = numerator / denominator;
})