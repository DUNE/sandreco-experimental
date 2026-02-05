#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <ocl/ocl.hpp>

#include <chrono>
#include <random>
#include <string>

#include <geoinfo/grain_info.hpp>
#include <hdf5/hdf5.hpp>
#include <mask_weights_computation.hpp>
#include <common/sand.h>

namespace sand::grain {

  class mask_weights_computation : public ufw::process {
   public:
    mask_weights_computation();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    void configure_frustum(cl::platform& platform);
    void configure_solidangle(cl::platform& platform);
    solidangle_cfg m_solidangle_cfg;
    static constexpr size_t s_max_platforms = 4;
    cl::Program m_frustum_program;
    cl::Kernel m_frustum_kernel;
    cl::Program m_solidangle_program;
    cl::Kernel m_solidangle_kernel;
  };

  void mask_weights_computation::configure_frustum(cl::platform& platform) {
    const char* frustum_kernel_src =
#include "../cl_src/common_structs.cl"
#include "../cl_src/common_functions.cl"
#include "../cl_src/make_frustum.cl"
        ;
    platform.build_program(m_frustum_program, frustum_kernel_src);
    m_frustum_kernel = cl::Kernel(m_frustum_program, "make_frustum");
  }

  void mask_weights_computation::configure_solidangle(cl::platform& platform) {
    const char* solidangle_kernel_src =
#include "../cl_src/common_structs.cl"
#include "../cl_src/common_functions.cl"
#include "../cl_src/solidangle.cl"
        ;
    platform.build_program(m_solidangle_program, solidangle_kernel_src);
    m_solidangle_kernel = cl::Kernel(m_solidangle_program, "solidangle");
  }

  void mask_weights_computation::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_solidangle_cfg = {cfg.at("voxel_size"), cfg.at("lar_attenuation_length"), cfg.at("pde"),
                        cfg.at("minivoxels_per_side")};
    auto& platform   = instance<cl::platform>();
    configure_frustum(platform);
    configure_solidangle(platform);
  }

  mask_weights_computation::mask_weights_computation() : process({}, {}) {
    UFW_DEBUG("Creating an mask_weights_computation process at {}.", fmt::ptr(this));
  }

  void mask_weights_computation::run() {
    UFW_DEBUG("Running an mask_weights_computation process at {}.", fmt::ptr(this));
    auto& platform = instance<cl::platform>();
    const auto& gi = instance<geoinfo>();

    dir_3d voxel_sizes(m_solidangle_cfg.voxel_size, m_solidangle_cfg.voxel_size, m_solidangle_cfg.voxel_size);
    auto voxels = gi.grain().fiducial_voxels(voxel_sizes);

    transform_t voxel_transform    = to_ocl_xform(voxels.xform_id_to_fiducial(voxel_sizes));
    const size_t sensor_rects_size = camera_height * camera_width;
    const size_t solidangle_size   = voxels.size().x() * voxels.size().y() * voxels.size().z() * sensor_rects_size;

    cl::NDRange solidangle_global_size(voxels.size().x(), voxels.size().y(), voxels.size().z());
    UFW_DEBUG("Solidangle global work size: ({},{},{})", solidangle_global_size[0], solidangle_global_size[1],
             solidangle_global_size[2]);

    // Setup output file
    auto& array = instance<sand::hdf5::ndarray>("angle_writer");
    sand::hdf5::ndarray::ndrange range(
        {solidangle_global_size[0], solidangle_global_size[1], solidangle_global_size[2], sensor_rects_size});
    range.set_type(H5::PredType::NATIVE_FLOAT);

    for (const auto& camera : gi.grain().mask_cameras()) {
      UFW_INFO("Processing camera: {}", camera.name);
      auto t_start = std::chrono::high_resolution_clock::now();

      const size_t mask_rects_size    = camera.holes.size();
      const size_t frustum_array_size = sensor_rects_size * mask_rects_size;
      std::unique_ptr<frustum_t[]> h_frustum_array(new frustum_t[frustum_array_size]);

      transform_t camera_transform = to_ocl_xform(camera.transform);

      cl_float z_mask    = static_cast<cl_float>(camera.z_mask);
      cl_float z_sensors = static_cast<cl_float>(camera.z_sipm);

      cl::buffer buf_sensor_rects;
      buf_sensor_rects.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY>(
          platform.context(), sensor_rects_size * sizeof(geoinfo::grain_info::rect_f),
          camera.sipm_active_areas.Array());

      cl::buffer buf_mask_rects;
      buf_mask_rects.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY>(
          platform.context(), mask_rects_size * sizeof(geoinfo::grain_info::rect_f), camera.holes.data());

      cl::buffer buf_frustum;
      buf_frustum.allocate<CL_MEM_READ_WRITE>(platform.context(), frustum_array_size * sizeof(frustum_t));

      // set kernel args
      try {
        m_frustum_kernel.setArg(0, camera_transform);
        m_frustum_kernel.setArg(1, 0);
        m_frustum_kernel.setArg(2, buf_mask_rects);
        m_frustum_kernel.setArg(3, z_mask);
        m_frustum_kernel.setArg(4, buf_sensor_rects);
        m_frustum_kernel.setArg(5, z_sensors);
        m_frustum_kernel.setArg(6, buf_frustum);
      } catch (const cl::Error& e) {
        UFW_WARN("OpenCL make_frustum Program Kernel setArg: {} ({})", e.what(), e.err());
        throw;
      }

      cl::NDRange global_size(mask_rects_size, sensor_rects_size);
      UFW_DEBUG("Frustum global work size: ({},{})", global_size[0], global_size[1]);
      cl::Event ev_frustum_kernel_execution;
      platform.queues().front().enqueueNDRangeKernel(m_frustum_kernel, cl::NullRange, cl::NDRange(global_size),
                                                     cl::NullRange, nullptr, &ev_frustum_kernel_execution);
      void* frustum_p = h_frustum_array.get();
      cl::Event ev_copy_frustum_from_device =
          buf_frustum.read(frustum_p, platform.queues().front(), 0, -1, {ev_frustum_kernel_execution});

      platform.queues().front().finish();

      std::unique_ptr<float[]> h_solidangle_array(new float[solidangle_size]);
      cl::buffer buf_solidangles;
      buf_solidangles.allocate<CL_MEM_WRITE_ONLY>(platform.context(), solidangle_size * sizeof(cl_float));

      // set kernel args
      try {
        m_solidangle_kernel.setArg(0, voxel_transform);
        m_solidangle_kernel.setArg(1, camera_transform);
        m_solidangle_kernel.setArg(2, buf_frustum);
        m_solidangle_kernel.setArg(3, static_cast<int>(mask_rects_size));
        m_solidangle_kernel.setArg(4, buf_mask_rects);
        m_solidangle_kernel.setArg(5, z_mask);
        m_solidangle_kernel.setArg(6, static_cast<int>(sensor_rects_size));
        m_solidangle_kernel.setArg(7, buf_sensor_rects);
        m_solidangle_kernel.setArg(8, z_sensors);
        m_solidangle_kernel.setArg(9, m_solidangle_cfg);
        m_solidangle_kernel.setArg(10, buf_solidangles);
      } catch (const cl::Error& e) {
        UFW_WARN("OpenCL solidangle Program Kernel setArg: {} ({})", e.what(), e.err());
        throw;
      }

      cl::Event ev_solidangle_kernel_execution;
      platform.queues().front().enqueueNDRangeKernel(m_solidangle_kernel, cl::NullRange,
                                                     cl::NDRange(solidangle_global_size), cl::NullRange, nullptr,
                                                     &ev_solidangle_kernel_execution);
      void* solidangle_p = h_solidangle_array.get();
      cl::Event ev_copy_solidangle_from_device =
          buf_solidangles.read(solidangle_p, platform.queues().front(), 0, -1, {ev_solidangle_kernel_execution});

      platform.queues().front().finish();

      // Write to hdf5
      array.write(camera.name, range, h_solidangle_array.get());
      auto t_stop = std::chrono::high_resolution_clock::now();
      double elapsed_time = std::chrono::duration<double>(t_stop - t_start).count();
      UFW_INFO("{} completed, time taken: {} s", camera.name, elapsed_time);
    }
  }
} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::mask_weights_computation)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::mask_weights_computation)
