CL_KERNEL(void solidangle(const transform_t voxel_id_to_grain, const transform_t camera_transform,
                          __global const frustum_t* frustums, const int n_holes, __global const rect_f* mask_rects,
                          const float z_mask, const int n_sensors, __global const rect_f* sensor_rects,
                          const float z_sensors, const solidangle_cfg cfg, __global float* solid_angles) {
  const int i         = get_global_id(0);
  const int j         = get_global_id(1);
  const int k         = get_global_id(2);
  const int voxel_idx = (i * get_global_size(1) + j) * get_global_size(2) + k;

  // voxel center in fiducial frame
  const float4 self_f         = transform4(voxel_id_to_grain, convert_float4((int4)(i, j, k, 1)));
  const float half_voxel_diag = cfg.voxel_size * 0.5; // cfg.voxel_size * sqrt(3.f) * 0.5;

  const int tot_minivoxels = cfg.minivoxels_per_side * cfg.minivoxels_per_side * cfg.minivoxels_per_side;

  const float2 zo_mask    = (float2)(z_mask, 1.f);
  const float2 zo_sensors = (float2)(z_sensors, 1.f);

  for (int sens_id = 0; sens_id < n_sensors; ++sens_id) {
    const int out_id = voxel_idx * n_sensors + sens_id;

    // find corners of the active pixel area in fiducial frame
    rect_f rect       = sensor_rects[sens_id];
    float4 sens_00_f  = transform4(camera_transform, (float4)(rect.left, rect.bottom, zo_sensors));
    float4 sens_01_f  = transform4(camera_transform, (float4)(rect.left, rect.top, zo_sensors));
    float4 sens_10_f  = transform4(camera_transform, (float4)(rect.right, rect.bottom, zo_sensors));
    float4 sens_11_f  = transform4(camera_transform, (float4)(rect.right, rect.top, zo_sensors));
    float4 sens_mid_f = transform4(
        camera_transform, (float4)((rect.left + rect.right) / 2.f, (rect.top + rect.bottom) / 2.f, zo_sensors));
    // sensor normal
    float4 sens_n_f            = normalize(cross(sens_11_f - sens_01_f, sens_00_f - sens_01_f));
    double angle               = 0.0;
    float distance_from_sensor = length(self_f - sens_mid_f);
    float attenuation_coeff    = exp(-(distance_from_sensor / cfg.lar_attenuation_length));

    // loop over mask holes
    for (int mask_id = 0; mask_id < n_holes; ++mask_id) {
      const int frustum_id = sens_id * n_holes + mask_id;
      float4 distances     = (float4)(dot(frustums[frustum_id].top_n, self_f - frustums[frustum_id].top_o),
                                  dot(frustums[frustum_id].rgt_n, self_f - frustums[frustum_id].rgt_o),
                                  dot(frustums[frustum_id].bot_n, self_f - frustums[frustum_id].bot_o),
                                  dot(frustums[frustum_id].lft_n, self_f - frustums[frustum_id].lft_o));
      float overlap_factor = 0.f;

      if (any(distances < -half_voxel_diag)) {
        continue;
      } else {
        // find corners of the mask opening in fiducial frame
        rect              = mask_rects[mask_id];
        float4 hole_01_f  = transform4(camera_transform, (float4)(rect.left, rect.top, zo_mask));
        float4 hole_10_f  = transform4(camera_transform, (float4)(rect.right, rect.bottom, zo_mask));
        float4 hole_11_f  = transform4(camera_transform, (float4)(rect.right, rect.top, zo_mask));
        float4 hole_00_f  = transform4(camera_transform, (float4)(rect.left, rect.bottom, zo_mask));
        float4 hole_mid_f = transform4(
            camera_transform, (float4)((rect.left + rect.right) / 2.f, (rect.top + rect.bottom) / 2.f, zo_mask));

        // Loop on minivoxels
        for (int mini_index_x = 0; mini_index_x < cfg.minivoxels_per_side; ++mini_index_x) {
          for (int mini_index_y = 0; mini_index_y < cfg.minivoxels_per_side; ++mini_index_y) {
            for (int mini_index_z = 0; mini_index_z < cfg.minivoxels_per_side; ++mini_index_z) {
              const float4 shift =
                  (float4)((-0.5 + ((float)mini_index_x + 0.5) / (float)cfg.minivoxels_per_side) * cfg.voxel_size,
                           (-0.5 + ((float)mini_index_y + 0.5) / (float)cfg.minivoxels_per_side) * cfg.voxel_size,
                           (-0.5 + ((float)mini_index_z + 0.5) / (float)cfg.minivoxels_per_side) * cfg.voxel_size, 0.f);
              const float4 mini_self_f = self_f + shift;

              // find projection of mask opening on sensor plane in fiducial coordinates
              float4 proj_00_f = hole_00_f
                               + (hole_00_f - mini_self_f)
                                     * (dot(sens_00_f - hole_00_f, sens_n_f) / dot(hole_00_f - mini_self_f, sens_n_f));
              float4 proj_01_f = hole_01_f
                               + (hole_01_f - mini_self_f)
                                     * (dot(sens_00_f - hole_01_f, sens_n_f) / dot(hole_01_f - mini_self_f, sens_n_f));
              float4 proj_10_f = hole_10_f
                               + (hole_10_f - mini_self_f)
                                     * (dot(sens_00_f - hole_10_f, sens_n_f) / dot(hole_10_f - mini_self_f, sens_n_f));
              float4 proj_11_f = hole_11_f
                               + (hole_11_f - mini_self_f)
                                     * (dot(sens_00_f - hole_11_f, sens_n_f) / dot(hole_11_f - mini_self_f, sens_n_f));

              // map 3d fiducial coordinates to 2d sensor plane coordinates (s = sensor)
              float4 ex_sf = normalize(sens_01_f - sens_00_f);
              float4 ey_sf = normalize(sens_10_f - sens_00_f);

              float2 proj_00_s = (float2)(dot(proj_00_f - sens_00_f, ex_sf), dot(proj_00_f - sens_00_f, ey_sf));
              float2 proj_11_s = (float2)(dot(proj_11_f - sens_00_f, ex_sf), dot(proj_11_f - sens_00_f, ey_sf));
              float2 proj_10_s = (float2)(dot(proj_10_f - sens_00_f, ex_sf), dot(proj_10_f - sens_00_f, ey_sf));
              float2 proj_01_s = (float2)(dot(proj_01_f - sens_00_f, ex_sf), dot(proj_01_f - sens_00_f, ey_sf));
              float2 sens_00_s = (float2)(dot(sens_00_f - sens_00_f, ex_sf), dot(sens_00_f - sens_00_f, ey_sf));
              float2 sens_11_s = (float2)(dot(sens_11_f - sens_00_f, ex_sf), dot(sens_11_f - sens_00_f, ey_sf));
              float2 sens_01_s = (float2)(dot(sens_01_f - sens_00_f, ex_sf), dot(sens_01_f - sens_00_f, ey_sf));
              float2 sens_10_s = (float2)(dot(sens_10_f - sens_00_f, ex_sf), dot(sens_10_f - sens_00_f, ey_sf));

              // find topright and bottomleft points (sensor frame)
              float2 sens_br_s;
              float2 sens_tl_s;
              float2 proj_br_s;
              float2 proj_tl_s;
              sens_br_s.x = fmax(sens_00_s.x, sens_11_s.x);
              sens_br_s.x = fmax(sens_br_s.x, sens_10_s.x);
              sens_br_s.x = fmax(sens_br_s.x, sens_01_s.x);

              sens_br_s.y = fmin(sens_00_s.y, sens_11_s.y);
              sens_br_s.y = fmin(sens_br_s.y, sens_10_s.y);
              sens_br_s.y = fmin(sens_br_s.y, sens_01_s.y);

              sens_tl_s.x = fmin(sens_00_s.x, sens_11_s.x);
              sens_tl_s.x = fmin(sens_tl_s.x, sens_10_s.x);
              sens_tl_s.x = fmin(sens_tl_s.x, sens_01_s.x);

              sens_tl_s.y = fmax(sens_00_s.y, sens_11_s.y);
              sens_tl_s.y = fmax(sens_tl_s.y, sens_10_s.y);
              sens_tl_s.y = fmax(sens_tl_s.y, sens_01_s.y);

              proj_br_s.x = fmax(proj_00_s.x, proj_11_s.x);
              proj_br_s.x = fmax(proj_br_s.x, proj_10_s.x);
              proj_br_s.x = fmax(proj_br_s.x, proj_01_s.x);

              proj_br_s.y = fmin(proj_00_s.y, proj_11_s.y);
              proj_br_s.y = fmin(proj_br_s.y, proj_10_s.y);
              proj_br_s.y = fmin(proj_br_s.y, proj_01_s.y);

              proj_tl_s.x = fmin(proj_00_s.x, proj_11_s.x);
              proj_tl_s.x = fmin(proj_tl_s.x, proj_10_s.x);
              proj_tl_s.x = fmin(proj_tl_s.x, proj_01_s.x);

              proj_tl_s.y = fmax(proj_00_s.y, proj_11_s.y);
              proj_tl_s.y = fmax(proj_tl_s.y, proj_10_s.y);
              proj_tl_s.y = fmax(proj_tl_s.y, proj_01_s.y);

              // find corners of sensor area intersecting with projection (sensor frame)
              /*
                A------B
                |      |
                |      |
                C------D

              */

              float2 A_s;
              float2 B_s;
              float2 C_s;
              float2 D_s;

              // horizontal

              int ACin       = 0;
              int BDin       = 0;
              int skip_angle = 0;

              if ((proj_tl_s.x >= sens_tl_s.x) && (proj_tl_s.x <= sens_br_s.x))
                ACin = 1;
              if ((proj_br_s.x >= sens_tl_s.x) && (proj_br_s.x <= sens_br_s.x))
                BDin = 1;

              if (ACin == 1 && BDin == 1) {
                A_s.x = proj_tl_s.x;
                C_s.x = A_s.x;
                B_s.x = proj_br_s.x;
                D_s.x = B_s.x;
              }

              else if (ACin == 0 && BDin == 1) {
                A_s.x = sens_tl_s.x;
                C_s.x = A_s.x;
                B_s.x = proj_br_s.x;
                D_s.x = B_s.x;
              }

              else if (ACin == 1 && BDin == 0) {
                A_s.x = proj_tl_s.x;
                C_s.x = A_s.x;
                B_s.x = sens_br_s.x;
                D_s.x = B_s.x;
              }

              else if (ACin == 0 && BDin == 0) {
                // mask projection and sensor not intersecting, assign same point coordinates to vertexes and raise flag
                if ((proj_tl_s.x >= sens_br_s.x && proj_br_s.x >= sens_br_s.x)
                    || (proj_tl_s.x <= sens_tl_s.x && proj_br_s.x <= sens_tl_s.x)) {
                  A_s.x      = sens_br_s.x;
                  C_s.x      = A_s.x;
                  B_s.x      = A_s.x;
                  D_s.x      = A_s.x;
                  skip_angle = 1;
                }
                // mask projection covering whole sensor area
                else {
                  A_s.x = sens_tl_s.x;
                  C_s.x = A_s.x;
                  B_s.x = sens_br_s.x;
                  D_s.x = B_s.x;
                }
              }

              // vertical

              int ABin = 0;
              int CDin = 0;

              if ((proj_tl_s.y <= sens_tl_s.y) && (proj_tl_s.y >= sens_br_s.y))
                ABin = 1;
              if ((proj_br_s.y <= sens_tl_s.y) && (proj_br_s.y >= sens_br_s.y))
                CDin = 1;

              if (ABin == 1 && CDin == 1) {
                A_s.y = proj_tl_s.y;
                B_s.y = A_s.y;
                C_s.y = proj_br_s.y;
                D_s.y = C_s.y;
              }

              else if (ABin == 0 && CDin == 1) {
                A_s.y = sens_tl_s.y;
                B_s.y = A_s.y;
                C_s.y = proj_br_s.y;
                D_s.y = C_s.y;
              }

              else if (ABin == 1 && CDin == 0) {
                A_s.y = proj_tl_s.y;
                B_s.y = A_s.y;
                C_s.y = sens_br_s.y;
                D_s.y = C_s.y;
              }

              else if (ABin == 0 && CDin == 0) {
                // mask projection and sensor not intersecting, assign same point coordinates to vertexes and raise flag
                if ((proj_tl_s.y <= sens_br_s.y && proj_br_s.y <= sens_br_s.y)
                    || (proj_tl_s.y >= sens_tl_s.y && proj_br_s.y >= sens_tl_s.y)) {
                  A_s.y      = sens_br_s.y;
                  C_s.y      = A_s.y;
                  B_s.y      = A_s.y;
                  D_s.y      = A_s.y;
                  skip_angle = 1;
                }
                // mask projection covering whole sensor area
                else {
                  A_s.y = sens_tl_s.y;
                  B_s.y = A_s.y;
                  C_s.y = sens_br_s.y;
                  D_s.y = C_s.y;
                }
              }

              if (!skip_angle) {
                // now we (hopefully) have vertex points of mask projection and sensor intersection
                // let's go back to fiducial coord  P_f = sens_00_f + P_s.x * ex_sf + P_s.y * ey_sf

                float4 A_f = sens_00_f + A_s.x * ex_sf + A_s.y * ey_sf;
                float4 B_f = sens_00_f + B_s.x * ex_sf + B_s.y * ey_sf;
                float4 C_f = sens_00_f + C_s.x * ex_sf + C_s.y * ey_sf;
                float4 D_f = sens_00_f + D_s.x * ex_sf + D_s.y * ey_sf;

                // solid angle subtended by triangular surface ( ABC + BCD)
                float4 rA = A_f - mini_self_f;
                float4 rB = B_f - mini_self_f;
                float4 rC = C_f - mini_self_f;
                float4 rD = D_f - mini_self_f;

                float num_ABC   = fabs(dot(rA, cross(rB, rC)));
                float denom_ABC = length(rA) * length(rB) * length(rC) + dot(rA, rB) * length(rC)
                                + dot(rA, rC) * length(rB) + dot(rB, rC) * length(rA);

                float num_BCD   = fabs(dot(rB, cross(rC, rD)));
                float denom_BCD = length(rB) * length(rC) * length(rD) + dot(rB, rC) * length(rD)
                                + dot(rB, rD) * length(rC) + dot(rC, rD) * length(rB);

                double ang_ABC;
                double ang_BCD;

                if (denom_ABC > 0) {
                  ang_ABC = 2 * atanpi(num_ABC / denom_ABC);
                } else if (denom_ABC < 0) {
                  ang_ABC = 2 * (atanpi(num_ABC / denom_ABC) + 1);
                } else {
                  ang_ABC = 0.f;
                }

                if (denom_BCD > 0) {
                  ang_BCD = 2 * atanpi(num_BCD / denom_BCD);
                } else if (denom_BCD < 0) {
                  ang_BCD = 2 * (atanpi(num_BCD / denom_BCD) + 1);
                } else {
                  ang_BCD = 0.f;
                }

                angle += (ang_ABC + ang_BCD) / 4.f;
              }
            }
          }
        }
      }
    }
    float mean_angle     = angle / (float)tot_minivoxels;
    solid_angles[out_id] = mean_angle * attenuation_coeff * cfg.pde;
  }
})
