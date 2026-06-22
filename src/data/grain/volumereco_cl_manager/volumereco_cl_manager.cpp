#include <geoinfo/grain_info.hpp>
#include <hdf5/hdf5.hpp>
#include <volumereco_cl_manager.hpp>

#include <ufw/context.hpp>

#include <unistd.h>

namespace sand::grain {

  volumereco_cl_manager::volumereco_cl_manager(const ufw::config& cfg)
    : r_platform(ufw::context::current()->instance<cl::platform>()) {
    m_n_devices  = r_platform.devices().size();
    m_voxel_size = cfg.at("voxel_size");
    configure_expectation();
    configure_maximization();
    configure_invert_matrix();
    configure_multiply_matrices_in_place();
    configure_add_matrices_in_place();
    const auto& gi       = ufw::context::current()->instance<geoinfo>();
    m_fiducial           = gi.grain().fiducial_voxels(sand::dir_3d(m_voxel_size, m_voxel_size, m_voxel_size));
    m_voxels_count       = m_fiducial.size().x() * m_fiducial.size().y() * m_fiducial.size().z();
    m_pixels_count       = camera_height * camera_width;
    size_t voxels_bytes  = m_voxels_count * sizeof(float);
    size_t weights_bytes = m_voxels_count * m_pixels_count * sizeof(float);
    UFW_INFO("Will allocate {} MB for voxel buffers", voxels_bytes / 1024 / 1024);
    UFW_INFO("Will allocate {} MB for each system_matrix buffer", weights_bytes / 1024 / 1024);
    for (const auto& camera : gi.grain().mask_cameras()) {
      m_system_matrix_buffers[camera.id].allocate<CL_MEM_READ_ONLY>(r_platform.context(), weights_bytes);
    }
    m_sensitivity_matrix_buffers          = std::vector<sand::cl::buffer>(m_n_devices);
    m_inverted_sensitivity_matrix_buffers = std::vector<sand::cl::buffer>(m_n_devices);
    m_expectation_buffers                 = std::vector<sand::cl::buffer>(m_n_devices);
    m_maximization_buffers                = std::vector<sand::cl::buffer>(m_n_devices);
    m_previous_amplitude_buffers          = std::vector<sand::cl::buffer>(m_n_devices);
    // TODO: see if any of these can gain CL_MEM_HOST_NO_ACCESS
    for (size_t idev = 0; idev < m_n_devices; ++idev) {
      m_sensitivity_matrix_buffers[idev].allocate<CL_MEM_READ_ONLY>(r_platform.context(), voxels_bytes);
      m_inverted_sensitivity_matrix_buffers[idev].allocate<CL_MEM_READ_WRITE>(r_platform.context(), voxels_bytes);
      m_expectation_buffers[idev].allocate<CL_MEM_READ_WRITE>(r_platform.context(), m_pixels_count * sizeof(float));
      m_maximization_buffers[idev].allocate<CL_MEM_READ_WRITE>(r_platform.context(), voxels_bytes);
      m_previous_amplitude_buffers[idev].allocate<CL_MEM_READ_WRITE>(r_platform.context(), voxels_bytes);
    }
  }

  void volumereco_cl_manager::load_weights(sand::hdf5::ndarray& weights) {
    const auto weights_dim_tmp = weights.range(weights.datasets().front());
    const size_4d weights_shape(weights_dim_tmp[0], weights_dim_tmp[1], weights_dim_tmp[2], weights_dim_tmp[3]);

    // Check that voxel shape matches between geometry and weights
    UFW_ASSERT(weights_shape.X() == m_fiducial.size().x() && weights_shape.Y() == m_fiducial.size().y()
                   && weights_shape.Z() == m_fiducial.size().z(),
               "hdf5 voxels shape: ({}, {}, {}). Geometry voxels shape: {}.", weights_shape.X(), weights_shape.Y(),
               weights_shape.Z(), m_fiducial.size());

    // Calculate combined sensitivity matrix
    std::vector<float> accumulate_sensitivity(m_voxels_count, 0.f);
    for (const auto& camera : weights.datasets()) {
      const auto camera_id = static_cast<channel_id::link_t>(std::stoi(weights.attribute(camera, "camera_id")));
      UFW_DEBUG("Loading camera {} with id {}", camera, camera_id);
      auto& system_buf = m_system_matrix_buffers.at(camera_id);
      auto mapped_ptr  = system_buf.map(r_platform.queues()[camera_id % m_n_devices]);
      weights.read(camera, static_cast<float*>(mapped_ptr.get()));
      // Sensitivity matrix is system matrix summed over the sensors
      auto sensitivity = get_sensitivity_from_system_matrix(static_cast<float*>(mapped_ptr.get()), weights_shape);
      UFW_ASSERT(accumulate_sensitivity.size() == sensitivity.size(), "sizes differ: {}, {}",
                 accumulate_sensitivity.size(), sensitivity.size());
      std::transform(accumulate_sensitivity.begin(), accumulate_sensitivity.end(), sensitivity.begin(),
                     accumulate_sensitivity.begin(), std::plus<float>());
    }
    wait();
    // Transfer combined sensitivity matrix and generate its inverse (1/element, not the matrix inverse)
    cl::NDRange global_size(m_fiducial.size().x(), m_fiducial.size().y(), m_fiducial.size().z());
    for (size_t idev = 0; idev < m_n_devices; ++idev) {
      cl::Event write = m_sensitivity_matrix_buffers[idev].write(accumulate_sensitivity, r_platform.queues()[idev]);
      m_inverted_sensitivity_matrix_buffers[idev];
      enqueue_on_device_after_with_args(
          m_invert_matrix_kernel, idev, cl::NullRange, global_size, cl::NullRange, {write},
          /* kernel args */
          m_sensitivity_matrix_buffers[idev], m_inverted_sensitivity_matrix_buffers[idev]);
    }
    wait();
  }

  void volumereco_cl_manager::configure_expectation() {
    const char* expectation_kernel_src =
#include "cl_src/expectation.cl"
        ;
    r_platform.build_program(m_expectation_program, expectation_kernel_src);
    m_expectation_kernel = cl::Kernel(m_expectation_program, "expectation");
  }

  void volumereco_cl_manager::configure_maximization() {
    const char* maximization_kernel_src =
#include "cl_src/maximization.cl"
        ;
    r_platform.build_program(m_maximization_program, maximization_kernel_src);
    m_maximization_kernel = cl::Kernel(m_maximization_program, "maximization");
  }

  void volumereco_cl_manager::configure_invert_matrix() {
    const char* invert_matrix_kernel_src =
#include "cl_src/invert_matrix.cl"
        ;
    r_platform.build_program(m_invert_matrix_program, invert_matrix_kernel_src);
    m_invert_matrix_kernel = cl::Kernel(m_invert_matrix_program, "invert_matrix");
  }

  void volumereco_cl_manager::configure_multiply_matrices_in_place() {
    const char* multiply_matrices_in_place_kernel_src =
#include "cl_src/multiply_matrices_in_place.cl"
        ;
    r_platform.build_program(m_multiply_matrices_in_place_program, multiply_matrices_in_place_kernel_src);
    m_multiply_matrices_in_place_kernel =
        cl::Kernel(m_multiply_matrices_in_place_program, "multiply_matrices_in_place");
  }

  void volumereco_cl_manager::configure_add_matrices_in_place() {
    const char* add_matrices_in_place_kernel_src =
#include "cl_src/add_matrices_in_place.cl"
        ;
    r_platform.build_program(m_add_matrices_in_place_program, add_matrices_in_place_kernel_src);
    m_add_matrices_in_place_kernel = cl::Kernel(m_add_matrices_in_place_program, "add_matrices_in_place");
  }

  std::vector<float> volumereco_cl_manager::get_sensitivity_from_system_matrix(const float* system_matrix,
                                                                               size_4d dimensions) {
    std::vector<float> sensitivity_matrix(dimensions.X() * dimensions.Y() * dimensions.Z(), 0.f);
    // sum over the sensor axis
    for (std::size_t d0 = 0; d0 < dimensions.X(); ++d0) {
      for (std::size_t d1 = 0; d1 < dimensions.Y(); ++d1) {
        for (std::size_t d2 = 0; d2 < dimensions.Z(); ++d2) {
          double sum{0.f};
          std::size_t voxel_index = (d0 * dimensions.Y() + d1) * dimensions.Z() + d2;
          for (std::size_t d3 = 0; d3 < dimensions.T(); ++d3) {
            sum += system_matrix[voxel_index * dimensions.T() + d3];
          }
          sensitivity_matrix[voxel_index] = sum;
        }
      }
    }
    return sensitivity_matrix;
  }

  void volumereco_cl_manager::wait() {
    for (const auto& queue : r_platform.queues()) {
      queue.finish();
    }
  }

} // namespace sand::grain
