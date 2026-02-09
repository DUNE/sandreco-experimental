CL_KERNEL(void maximization(const transform_t camera_transform, int camera_id, __global const rect_f* mask_rects,
                            const float z_mask, __global const rect_f* sensor_rects, const float z_sensors,
                            __global frustum_t* frustums) {
  const int mask_rect_id  = get_global_id(0);
  const int sens_rect_id  = get_global_id(1);
  const int output_id     = sens_rect_id * get_global_size(0) + mask_rect_id;
  const float2 zo_mask    = (float2)(z_mask, 1.f);
  const float2 zo_sensors = (float2)(z_sensors, 1.f);

  // find corners of the mask opening in fiducial frame
  rect_f rect      = mask_rects[mask_rect_id];
  float4 hole_01_g = transform4(camera_transform, (float4)(rect.left, rect.top, zo_mask));
  float4 hole_10_g = transform4(camera_transform, (float4)(rect.right, rect.bottom, zo_mask));
  float4 hole_11_g = transform4(camera_transform, (float4)(rect.right, rect.top, zo_mask));
  float4 hole_00_g = transform4(camera_transform, (float4)(rect.left, rect.bottom, zo_mask));
  float4 hole_mid_g =
      transform4(camera_transform, (float4)((rect.left + rect.right) / 2.f, (rect.top + rect.bottom) / 2.f, zo_mask));

  // find corners of the active pixel area in fiducial frame
  rect              = sensor_rects[sens_rect_id];
  float4 sens_00_g  = transform4(camera_transform, (float4)(rect.left, rect.bottom, zo_sensors));
  float4 sens_01_g  = transform4(camera_transform, (float4)(rect.left, rect.top, zo_sensors));
  float4 sens_10_g  = transform4(camera_transform, (float4)(rect.right, rect.bottom, zo_sensors));
  float4 sens_11_g  = transform4(camera_transform, (float4)(rect.right, rect.top, zo_sensors));
  float4 sens_mid_g = transform4(camera_transform,
                                 (float4)((rect.left + rect.right) / 2.f, (rect.top + rect.bottom) / 2.f, zo_sensors));

  // compute the four limiting planes expressed as point + normal (pointing inside frustum) in fiducial frame
  frustums[output_id].top_o = hole_00_g;
  frustums[output_id].top_n = normalize(cross(sens_11_g - hole_00_g, sens_01_g - hole_10_g));
  frustums[output_id].rgt_o = hole_10_g;
  frustums[output_id].rgt_n = normalize(cross(sens_01_g - hole_10_g, sens_00_g - hole_11_g));
  frustums[output_id].bot_o = hole_11_g;
  frustums[output_id].bot_n = normalize(cross(sens_00_g - hole_11_g, sens_10_g - hole_01_g));
  frustums[output_id].lft_o = hole_01_g;
  frustums[output_id].lft_n = normalize(cross(sens_10_g - hole_01_g, sens_11_g - hole_00_g));
  frustums[output_id].idx   = (int3)(camera_id, mask_rect_id, sens_rect_id);
})
