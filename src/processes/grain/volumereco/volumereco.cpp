#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <ocl/ocl.hpp>

#include <chrono>
#include <random>
#include <string>
#include <algorithm>
#include <functional>

#include <geoinfo/grain_info.hpp>
#include <hdf5/hdf5.hpp>
#include <common/sand.h>
#include <grain/image.h>
#include <grain/grain.h>

namespace sand::grain {

  class volumereco : public ufw::process {
   public:
    volumereco();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    void configure_expectation(cl::platform& platform);
    void configure_maximization(cl::platform& platform);
    template <typename T>
    std::vector<T> get_sensitivity_from_system_matrix(const std::vector<T>& system_matrix, sand::hdf5::ndarray::ndrange dimensions);
    static constexpr size_t s_max_platforms = 4;
    float m_voxel_size;
    cl::Program m_expectation_program;
    cl::Kernel m_expectation_kernel;
    cl::Program m_maximization_program;
    cl::Kernel m_maximization_kernel;
    std::vector<cl::buffer> m_sensitivity_matrix_buffers; // One per GPU
    std::map<std::string, cl::buffer> m_system_matrix_buffers; // One per camera
    std::vector<cl::buffer> m_image_buffers; // One per GPU
    std::vector<cl::buffer> m_expectation_buffers; // One per GPU
    std::vector<cl::buffer> m_maximization_buffers; // One per GPU
    std::vector<cl::buffer> m_previous_amplitude_buffers; // One per GPU
  };

  template <typename T>
  std::vector<T> volumereco::get_sensitivity_from_system_matrix(const std::vector<T>& system_matrix, sand::hdf5::ndarray::ndrange dimensions)
  {
      assert(system_matrix.size() == dimensions.flat_size());
      std::vector<T> sensitivity_matrix(dimensions[0]*dimensions[1]*dimensions[2], T{});

      for (std::size_t d0 = 0; d0 < dimensions[0]; ++d0) {
          for (std::size_t d1 = 0; d1 < dimensions[1]; ++d1) {
              for (std::size_t d2 = 0; d2 < dimensions[2]; ++d2) {
                  T sum = T{};
                  std::size_t voxel_index = (d0 * dimensions[1] + d1) * dimensions[2] + d2;
                  for (std::size_t d3 = 0; d3 < dimensions[3]; ++d3) {
                      sum += system_matrix[voxel_index * dimensions[3] + d3];
                  }
                  sensitivity_matrix[voxel_index] = sum;
              }
          }
      }
      return sensitivity_matrix;
  }

  void volumereco::configure_expectation(cl::platform& platform) {
    const char* expectation_kernel_src =
#include "cl_src/expectation.cl"
        ;
    platform.build_program(m_expectation_program, expectation_kernel_src);
    m_expectation_kernel = cl::Kernel(m_expectation_program, "expectation");
  }

  void volumereco::configure_maximization(cl::platform& platform) {
    const char* maximization_kernel_src =
#include "cl_src/maximization.cl"
        ;
    platform.build_program(m_maximization_program, maximization_kernel_src);
    m_maximization_kernel = cl::Kernel(m_maximization_program, "maximization");
  }

  void volumereco::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_voxel_size = cfg.at("voxel_size");
    auto& platform   = instance<cl::platform>();
    configure_expectation(platform);
    configure_maximization(platform);

    auto& array = instance<sand::hdf5::ndarray>("angle_reader");
    const auto angle_dimensions = array.range(array.datasets().front());
    const size_t angle_size = angle_dimensions.flat_size();
    const size_t sensitivity_size = angle_dimensions[0]*angle_dimensions[1]*angle_dimensions[2];

    std::vector<float> temp_system(angle_size, 0.f);
    std::vector<float> temp_sensitivity(sensitivity_size, 0.f);
    for (const auto& camera : array.datasets()) {
      UFW_DEBUG("Loading camera {}", camera);
      array.read(camera, temp_system);
      auto& system_buf = m_system_matrix_buffers[camera];
      system_buf.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY>(
          platform.context(), angle_size * sizeof(float), temp_system.data());
      std::transform(temp_sensitivity.begin(), temp_sensitivity.end(),
                   get_sensitivity_from_system_matrix<float>(temp_system, angle_dimensions).begin(),
                   temp_sensitivity.begin(),
                   std::plus<float>());
    }

    const size_t n_devices = platform.devices().size();
    UFW_DEBUG("n_devices: {}", n_devices);
    m_sensitivity_matrix_buffers = std::vector<sand::cl::buffer>(n_devices);
    m_image_buffers = std::vector<sand::cl::buffer>(n_devices);
    m_expectation_buffers = std::vector<sand::cl::buffer>(n_devices);
    m_maximization_buffers = std::vector<sand::cl::buffer>(n_devices);
    m_previous_amplitude_buffers = std::vector<sand::cl::buffer>(n_devices);

    for (size_t i_device; i_device < n_devices; ++i_device) {
      UFW_DEBUG("Creating buffers on device {}", i_device);
      auto& sensitivity_buf = m_sensitivity_matrix_buffers[i_device];
      sensitivity_buf.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY>(
          platform.context(), sensitivity_size * sizeof(float), temp_sensitivity.data());

      auto& image_buf = m_image_buffers[i_device];
      image_buf.allocate<CL_MEM_READ_ONLY>(platform.context(), camera_height * camera_width * sizeof(float));

      auto& expectation_buf = m_expectation_buffers[i_device];
      expectation_buf.allocate<CL_MEM_READ_ONLY>(platform.context(), sensitivity_size * sizeof(float));

      auto& maximization_buf = m_maximization_buffers[i_device];
      maximization_buf.allocate<CL_MEM_READ_ONLY>(platform.context(), sensitivity_size * sizeof(float));

      auto& previous_amplitude_buf = m_previous_amplitude_buffers[i_device];
      previous_amplitude_buf.allocate<CL_MEM_READ_ONLY>(platform.context(), sensitivity_size * sizeof(float));
    }
  }

  volumereco::volumereco() : process({{"images", "sand::grain::images"}}, {}) {
    UFW_DEBUG("Creating a volumereco process at {}.", fmt::ptr(this));
  }

  void volumereco::run() {
    UFW_DEBUG("Running a volumereco process at {}.", fmt::ptr(this));
    const auto& images_in = get<images>("images");
    //auto& voxel_out = set<voxel_array<float>>("voxels");

    const auto& gi = instance<geoinfo>();

    dir_3d voxel_sizes(m_voxel_size, m_voxel_size, m_voxel_size);
    auto voxels = gi.grain().fiducial_voxels(voxel_sizes);
  }
} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::volumereco)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::volumereco)
