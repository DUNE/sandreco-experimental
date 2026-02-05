CL_FUNCTION(float3 transform3(transform_t t, float3 v) { return (t.x * v.x + t.y * v.y + t.z * v.z + t.w * 1.f).xyz; })

CL_FUNCTION(float4 transform4(transform_t t, float4 v) { return t.x * v.x + t.y * v.y + t.z * v.z + t.w * v.w; })