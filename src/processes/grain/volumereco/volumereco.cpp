#include <ocl/ocl.hpp>

#include <volumereco_cl_manager/volumereco_cl_manager.hpp>

#include <unordered_map>
#include <vector>

#include <hdf5/hdf5.hpp>
#include <common/sand.h>
#include <grain/grain.h>
#include <grain/image.h>
#include <grain/voxels.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::grain {

  class volumereco : public ufw::process {
   public:
    volumereco();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    template <typename T>
    std::vector<T> get_sensitivity_from_system_matrix(const T* p_system_matrix, size_4d dimensions);
    static constexpr size_t s_max_platforms = 4;
    size_t m_n_devices;
    size_t m_max_iterations;
    float m_pde;
    std::vector<cl::buffer> m_image_buffers; // One per camera
  };

  void volumereco::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_max_iterations = cfg.at("max_iterations");
    m_pde            = cfg.at("pde");
    auto& cl_manager = instance<volumereco_cl_manager>();
    m_n_devices      = cl_manager.platform().devices().size();
    auto& array      = instance<sand::hdf5::ndarray>("angle_reader");
    cl_manager.load_weights(array);
    // Buffers for images
    m_image_buffers = std::vector<sand::cl::buffer>(array.datasets().size());
    for (auto& image_buffer : m_image_buffers) {
      image_buffer.allocate<CL_MEM_READ_WRITE>(cl_manager.platform().context(),
                                               camera_height * camera_width * sizeof(float));
    }
  }

  volumereco::volumereco()
    : process({{"images", "sand::grain::images"}}, {{"photon_amplitudes", "sand::grain::voxels"}}) {}

  void volumereco::run() {
    const auto& spill_images_in = get<images>("images");
    auto& photon_amplitude_out  = set<voxels>("photon_amplitudes");
    auto& cl_manager            = instance<volumereco_cl_manager>();

    auto voxels            = cl_manager.fiducial();
    const size_t n_voxels  = voxels.size().x() * voxels.size().y() * voxels.size().z();
    const size_t n_sensors = camera_height * camera_width;
    const cl::NDRange voxel_shape(voxels.size().x(), voxels.size().y(), voxels.size().z());
    const cl::NDRange sensors_shape(n_sensors);

    std::vector<float> starting_score(n_voxels, 1.f);
    std::vector<float> starting_maximization(n_voxels, 0.f);

    int i_event_in_spill{0};

    for (const auto& images_in : spill_images_in.images) {
      if (!images_in.empty()) {
        for (const auto& image : images_in) {
          UFW_DEBUG("Image id: {}", image.camera_id);
          // Sharing load evenly among GPUs, assuming camera ids range [0, n_cameras -1]
          const size_t idev = image.camera_id % m_n_devices;
          UFW_DEBUG("Processing on device {}", idev);
          // Copy data to device buffer
          m_image_buffers[image.camera_id].write(image.amplitude_array<float>().Array(),
                                                 cl_manager.platform().queues()[image.camera_id % m_n_devices]);
          // Start with uniform voxel score distribution
          cl_manager.previous_amplitudes()[image.camera_id % m_n_devices].write(
              starting_score.data(), cl_manager.platform().queues()[image.camera_id % m_n_devices]);
        }
        for (int iteration = 0; iteration < m_max_iterations; ++iteration) {
          UFW_INFO("Iteration: {}", iteration);
          // Fill maximization buffers with 0
          for (size_t idev = 0; idev < m_n_devices; ++idev) {
            cl_manager.maximization_buffers()[idev].write(starting_maximization, cl_manager.platform().queues()[idev]);
          }
          cl_manager.wait();
          for (const auto& image : images_in) {
            const size_t idev = image.camera_id % m_n_devices;
            // Expectation step
            cl::Event ev_expectation_kernel_execution = cl_manager.enqueue_on_device_with_args(
                cl_manager.expectation(), idev, cl::NullRange, sensors_shape, cl::NullRange,
                /*kernel args*/
                cl_manager.system_matrix(image.camera_id), cl_manager.inverted_sensitivity()[idev],
                static_cast<int>(n_voxels), m_pde, cl_manager.previous_amplitudes()[idev],
                cl_manager.expectation_buffers()[idev]);

            // Maximization step
            cl::Events maximization_wait_for{ev_expectation_kernel_execution};
            cl::Event ev_maximization_kernel_execution = cl_manager.enqueue_on_device_after_with_args(
                cl_manager.maximization(), idev, cl::NullRange, voxel_shape, cl::NullRange, maximization_wait_for,
                /*kernel args*/
                cl_manager.system_matrix(image.camera_id), cl_manager.inverted_sensitivity()[idev],
                static_cast<int>(n_sensors), m_pde, cl_manager.expectation_buffers()[idev],
                m_image_buffers[image.camera_id], cl_manager.maximization_buffers()[idev]);
          }
          // Be sure that all GPU computations are completed
          cl_manager.wait();
          // Now we have n_devices results from maximization step that need to be summed together
          for (size_t idev = 1; idev < m_n_devices; ++idev) {
            cl::Event ev_add_matrices_in_place = cl_manager.enqueue_on_device_with_args(
                cl_manager.add_matrices_in_place(), 0, cl::NullRange, voxel_shape, cl::NullRange,
                /*kernel args*/
                cl_manager.maximization_buffers()[0], cl_manager.maximization_buffers()[idev]);
            cl_manager.platform().queues()[0].finish();
          }
          // Update amplitudes
          for (size_t idev = 0; idev < m_n_devices; ++idev) {
            cl::Event ev_multiply_matrices_in_place = cl_manager.enqueue_on_device_with_args(
                cl_manager.multiply_matrices_in_place(), idev, cl::NullRange, voxel_shape, cl::NullRange,
                /*kernel args*/
                cl_manager.previous_amplitudes()[idev], cl_manager.maximization_buffers()[0]);
          }
          cl_manager.wait();
        }
        // Before saving the output, the amplitudes must be multiplied by inverted_sensitivity_matrix
        cl::Event ev_multiply_matrices_in_place = cl_manager.enqueue_on_device_with_args(
            cl_manager.multiply_matrices_in_place(), 0, cl::NullRange, voxel_shape, cl::NullRange,
            /*kernel args*/
            cl_manager.previous_amplitudes()[0], cl_manager.inverted_sensitivity()[0]);
        // Retrieve voxel score
        photon_amplitude_out.voxels.emplace_back(voxels.size());
        cl_manager.previous_amplitudes()[0].read(photon_amplitude_out.voxels.back(), cl_manager.platform().queues()[0],
                                                 0, -1, {ev_multiply_matrices_in_place});

        cl_manager.wait();
      } else {
        UFW_DEBUG("Event {} slice {} has no images.", ufw::context::current()->id(), i_event_in_spill);
        photon_amplitude_out.voxels.emplace_back(voxels.size(), 0.0);
      }

      // Write to hdf5
      UFW_INFO("Write hdf5 for event {} slice {}", ufw::context::current()->id(), i_event_in_spill);
      auto& score_writer = instance<sand::hdf5::ndarray>("score_writer");
      sand::hdf5::ndarray::ndrange range({voxels.size().x(), voxels.size().y(), voxels.size().z()});
      range.set_type(H5::PredType::NATIVE_FLOAT);
      score_writer.write(std::to_string(ufw::context::current()->id()) + std::string("_")
                             + std::to_string(i_event_in_spill),
                         range, photon_amplitude_out.voxels.back().data());
      i_event_in_spill++;
    }
  }
} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::volumereco)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::volumereco)
