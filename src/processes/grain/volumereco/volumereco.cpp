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
#include <unordered_map>
#include <vector>

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
    void configure_invert_matrix(cl::platform& platform);
    void configure_multiply_matrices(cl::platform& platform);
    template <typename T>
    std::vector<T> get_sensitivity_from_system_matrix(const std::vector<T>& system_matrix, sand::hdf5::ndarray::ndrange dimensions);
    static constexpr size_t s_max_platforms = 4;
    size_t m_n_devices;
    size_t m_max_iterations;
    float m_voxel_size;
    float m_pde;
    cl::Program m_expectation_program;
    cl::Kernel m_expectation_kernel;
    cl::Program m_maximization_program;
    cl::Kernel m_maximization_kernel;
    cl::Program m_invert_matrix_program;
    cl::Kernel m_invert_matrix_kernel;
    cl::Program m_multiply_matrices_program;
    cl::Kernel m_multiply_matrices_kernel;
    cl::buffer m_sensitivity_matrix_buffer; // One
    std::vector<cl::buffer> m_inverted_sensitivity_matrix_buffers; // One per GPU
    std::unordered_map<channel_id::link_t, cl::buffer> m_system_matrix_buffers; // One per camera
    std::vector<cl::buffer> m_image_buffers; // One per GPU
    std::vector<cl::buffer> m_expectation_buffers; // One per GPU
    std::vector<cl::buffer> m_maximization_buffers; // One per GPU
    std::vector<cl::buffer> m_previous_amplitude_buffers; // One per GPU
  };

  template <typename T>
  std::vector<T> volumereco::get_sensitivity_from_system_matrix(const std::vector<T>& system_matrix, sand::hdf5::ndarray::ndrange dimensions)
  {
      UFW_ASSERT(system_matrix.size() == dimensions.flat_size(), "System and sensitivity matrices dimensions do not match");
      std::vector<T> sensitivity_matrix(dimensions[0]*dimensions[1]*dimensions[2], T{});
      // sum over the sensor axis
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

  void volumereco::configure_invert_matrix(cl::platform& platform) {
    const char* invert_matrix_kernel_src =
  #include "cl_src/invert_matrix.cl"
        ;
    platform.build_program(m_invert_matrix_program, invert_matrix_kernel_src);
    m_invert_matrix_kernel = cl::Kernel(m_invert_matrix_program, "invert_matrix");
  }

  void volumereco::configure_multiply_matrices(cl::platform& platform) {
    const char* multiply_matrices_kernel_src =
  #include "cl_src/multiply_matrices.cl"
        ;
    platform.build_program(m_multiply_matrices_program, multiply_matrices_kernel_src);
    m_multiply_matrices_kernel = cl::Kernel(m_multiply_matrices_program, "multiply_matrices");
  }

  void volumereco::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_voxel_size = cfg.at("voxel_size");
    m_max_iterations = cfg.at("max_iterations");
    m_pde = cfg.at("pde");
    auto& platform   = instance<cl::platform>();
    m_n_devices = platform.devices().size();
    configure_expectation(platform);
    configure_maximization(platform);
    configure_invert_matrix(platform);
    configure_multiply_matrices(platform);

    auto& array = instance<sand::hdf5::ndarray>("angle_reader");
    const auto angle_dimensions = array.range(array.datasets().front());
    const size_t angle_size = angle_dimensions.flat_size();
    const size_t sensitivity_size = angle_dimensions[0]*angle_dimensions[1]*angle_dimensions[2];

    // Check that voxel shape matches between geometry and weights
    const auto& gi = instance<geoinfo>();
    dir_3d voxel_sizes(m_voxel_size, m_voxel_size, m_voxel_size);
    auto voxels = gi.grain().fiducial_voxels(voxel_sizes);
    UFW_ASSERT(angle_dimensions[0]==voxels.size().x() && angle_dimensions[1]==voxels.size().y() && angle_dimensions[2]==voxels.size().z(), 
          "hdf5 voxels shape: {}, {}, {}. Geometry voxels shape: {}, {}, {}.", 
          angle_dimensions[0], angle_dimensions[1], angle_dimensions[2], voxels.size().x(), voxels.size().y(), voxels.size().z());

    std::vector<float> temp_system(angle_size, 0.f);
    std::vector<float> temp_sensitivity(sensitivity_size, 0.f);
    for (const auto& camera : array.datasets()) {
      // Tricky conversion from std::string to channel_id::link_t (uint8_t)
      const auto camera_id = static_cast<channel_id::link_t>(std::stoi(array.attribute(camera, "camera_id")));
      UFW_DEBUG("Loading camera {} with id {}", camera, camera_id);
      array.read(camera, temp_system);
      auto& system_buf = m_system_matrix_buffers[camera_id];
      system_buf.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY>(
          platform.context(), angle_size * sizeof(float), temp_system.data());
      // Sensitivity matrix is system matrix summed over the sensors
      std::transform(temp_sensitivity.begin(), temp_sensitivity.end(),
                   get_sensitivity_from_system_matrix<float>(temp_system, angle_dimensions).begin(),
                   temp_sensitivity.begin(),
                   std::plus<float>());
    }

    // Only one sensitivity matrix is needed
    m_sensitivity_matrix_buffer.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY>(
          platform.context(), sensitivity_size * sizeof(float), temp_sensitivity.data());

    // Create buffers across all GPUs
    UFW_DEBUG("n_devices: {}", m_n_devices);
    m_inverted_sensitivity_matrix_buffers = std::vector<sand::cl::buffer>(m_n_devices);
    m_image_buffers = std::vector<sand::cl::buffer>(m_n_devices);
    m_expectation_buffers = std::vector<sand::cl::buffer>(m_n_devices);
    m_maximization_buffers = std::vector<sand::cl::buffer>(m_n_devices);
    m_previous_amplitude_buffers = std::vector<sand::cl::buffer>(m_n_devices);

    for (size_t i_device = 0; i_device < m_n_devices; ++i_device) {
      UFW_DEBUG("Creating buffers on device {}", i_device);
      // Invert sensitivity matrix for faster computations later
      auto& inverted_sensitivity_buf = m_inverted_sensitivity_matrix_buffers[i_device];
      inverted_sensitivity_buf.allocate<CL_MEM_READ_WRITE>(platform.context(), sensitivity_size * sizeof(float));
      try {
        m_invert_matrix_kernel.setArg(0, m_sensitivity_matrix_buffer);
        m_invert_matrix_kernel.setArg(1, inverted_sensitivity_buf);
      } catch (const cl::Error& e) {
        UFW_WARN("OpenCL invert_matrix Program Kernel setArg: {} ({})", e.what(), e.err());
        throw;
      }
      cl::NDRange global_size(angle_dimensions[0], angle_dimensions[1], angle_dimensions[2]);
      cl::Event ev_invert_matrix_kernel_execution;
      platform.queues()[i_device].enqueueNDRangeKernel(m_invert_matrix_kernel, cl::NullRange, global_size,
                                                     cl::NullRange, nullptr, &ev_invert_matrix_kernel_execution);

      auto& image_buf = m_image_buffers[i_device];
      image_buf.allocate<CL_MEM_READ_WRITE>(platform.context(), camera_height * camera_width * sizeof(float));

      auto& expectation_buf = m_expectation_buffers[i_device];
      expectation_buf.allocate<CL_MEM_READ_WRITE>(platform.context(), camera_height * camera_width * sizeof(float));

      auto& maximization_buf = m_maximization_buffers[i_device];
      maximization_buf.allocate<CL_MEM_READ_WRITE>(platform.context(), sensitivity_size * sizeof(float));

      auto& previous_amplitude_buf = m_previous_amplitude_buffers[i_device];
      previous_amplitude_buf.allocate<CL_MEM_READ_WRITE>(platform.context(), sensitivity_size * sizeof(float));
    }

    // Be sure that all GPU computations are completed
    for (const auto& queue : platform.queues()) {
      queue.finish();
    }
  }

  volumereco::volumereco() : process({{"images", "sand::grain::images"}}, {}) {
    UFW_DEBUG("Creating a volumereco process at {}.", fmt::ptr(this));
  }

  void volumereco::run() {
    UFW_DEBUG("Running a volumereco process at {}.", fmt::ptr(this));
    const auto& images_in = get<images>("images");
    //auto& voxel_out = set<voxel_array<float>>("voxels");
    auto& platform = instance<cl::platform>();
    const auto& gi = instance<geoinfo>();

    dir_3d voxel_sizes(m_voxel_size, m_voxel_size, m_voxel_size);
    auto voxels = gi.grain().fiducial_voxels(voxel_sizes);
    const cl::NDRange voxel_shape(voxels.size().x(), voxels.size().y(), voxels.size().z());
    const cl::NDRange sensors_shape(camera_height * camera_width);
    const size_t n_voxels = voxels.size().x() * voxels.size().y() * voxels.size().z();
    std::vector<float> starting_score(n_voxels, 1.f);

    for (const auto& image : images_in.images) {
      UFW_DEBUG("Image id: {}", image.camera_id);
      if (m_system_matrix_buffers.count(image.camera_id) == 0) {
        UFW_ERROR("Camera id {} not found in buffers", image.camera_id);
      }
      // Sharing load evenly among GPUs, assuming camera ids are uniform
      const size_t device_index = image.camera_id % m_n_devices; 
      UFW_DEBUG("Processing on device {}", device_index);
      // Copy data to device buffer
      cl::Event ev_copy_image_to_buffer = m_image_buffers[image.camera_id % m_n_devices].write(image.amplitude_array<float>().Array() , platform.queues()[image.camera_id % m_n_devices], 0, -1);
      // Start with uniform voxel score distribution
      cl::Event ev_fill_previous_amplitude_buffer = m_previous_amplitude_buffers[image.camera_id % m_n_devices].write(starting_score.data(), platform.queues()[image.camera_id % m_n_devices], 0, -1);

      for (int iteration = 0; iteration < m_max_iterations; ++iteration) {
        UFW_DEBUG("Iteration: {}", iteration);
        // Expectation step
        try {
          m_expectation_kernel.setArg(0, m_system_matrix_buffers[image.camera_id]);
          m_expectation_kernel.setArg(1, m_inverted_sensitivity_matrix_buffers[device_index]);
          m_expectation_kernel.setArg(2, static_cast<int>(n_voxels));
          m_expectation_kernel.setArg(3, m_pde);
          m_expectation_kernel.setArg(4, m_previous_amplitude_buffers[device_index]);
          m_expectation_kernel.setArg(5, m_expectation_buffers[device_index]);
        } catch (const cl::Error& e) {
          UFW_WARN("OpenCL expectation Program Kernel setArg: {} ({})", e.what(), e.err());
          throw;
        }
        cl::Event ev_expectation_kernel_execution;
        platform.queues()[device_index].enqueueNDRangeKernel(m_expectation_kernel, cl::NullRange, sensors_shape,
                                                     cl::NullRange, nullptr, &ev_expectation_kernel_execution);
        
        // Maximization step

        // platform.queues()[device_index].finish();

      }

    }
    // Be sure that all GPU computations are completed
    for (const auto& queue : platform.queues()) {
      queue.finish();
    }
  }
} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::volumereco)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::volumereco)
