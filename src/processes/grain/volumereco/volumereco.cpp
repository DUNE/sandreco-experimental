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
    m_maximization_kernel = cl::Kernel(m_maximization_program, "solidangle");
  }

  void volumereco::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_voxel_size = cfg.at("voxel_size");
    auto& platform   = instance<cl::platform>();
    // configure_expectation(platform);
    // configure_maximization(platform);

    auto& array = instance<sand::hdf5::ndarray>("angle_reader");
    size_t angle_size = array.range(array.datasets().front()).flat_size();
    std::vector<float> temp_vector(angle_size, 0.f);
    for (const auto& camera : array.datasets()) {
      UFW_DEBUG("Loading camera {}", camera);
      array.read(camera, temp_vector);
      auto& buf = m_system_matrix_buffers[camera];
      buf.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY>(
          platform.context(), angle_size * sizeof(float), temp_vector.data());
    }
  }

  volumereco::volumereco() : process({{"images", "sand::grain::images"}}, {{"voxels", "sand::grain::voxel_array<float>"}}) {
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
