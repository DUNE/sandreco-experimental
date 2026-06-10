#include <hdf5/hdf5.hpp>
#include <ocl/ocl.hpp>
#include <common/data.h>
#include <common/sand.h>
#include <grain/grain.h>
#include <grain/image.h>
#include <grain/voxels.h>

namespace sand::grain {

  struct volumereco_cl_manager
    : public ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {
   public:
    explicit volumereco_cl_manager(const ufw::config&);

    cl::platform& platform() { return r_platform; }

    cl::Kernel& expectation() { return m_expectation_kernel; }
    cl::Kernel& maximization() { return m_maximization_kernel; }
    cl::Kernel& invert_matrix() { return m_invert_matrix_kernel; }
    cl::Kernel& multiply_matrices_in_place() { return m_multiply_matrices_in_place_kernel; }
    cl::Kernel& add_matrices_in_place() { return m_add_matrices_in_place_kernel; }

    voxel_array<uint8_t> fiducial() const { return m_fiducial; }

    void wait();

    template <typename... Args>
    cl::Event enqueue_on_device_with_args(cl::Kernel& kernel, size_t devidx, const cl::NDRange& offset,
                                          const cl::NDRange& global, const cl::NDRange& local, Args&&... args) {
      set_args(kernel, 0, std::forward<Args>(args)...);
      cl::Event evt;
      r_platform.queues()[devidx].enqueueNDRangeKernel(kernel, offset, global, local, nullptr, &evt);
      return std::move(evt);
    }

    template <typename... Args>
    cl::Event enqueue_on_device_after_with_args(cl::Kernel& kernel, size_t devidx, const cl::NDRange& offset,
                                                const cl::NDRange& global, const cl::NDRange& local,
                                                const cl::Events& after, Args&&... args) {
      set_args(kernel, 0, std::forward<Args>(args)...);
      cl::Event evt;
      r_platform.queues()[devidx].enqueueNDRangeKernel(kernel, offset, global, local, &after, &evt);
      return std::move(evt);
    }

    void load_weights(sand::hdf5::ndarray&);

    cl::buffer& system_matrix(sand::channel_id::link_t id) { return m_system_matrix_buffers.at(id); }

    std::vector<cl::buffer>& previous_amplitudes() { return m_previous_amplitude_buffers; }

    std::vector<cl::buffer>& maximization_buffers() { return m_maximization_buffers; }

    std::vector<cl::buffer>& inverted_sensitivity() { return m_inverted_sensitivity_matrix_buffers; }

    std::vector<cl::buffer>& expectation_buffers() { return m_expectation_buffers; }

   private:
    void configure_expectation();
    void configure_maximization();
    void configure_invert_matrix();
    void configure_multiply_matrices_in_place();
    void configure_add_matrices_in_place();

    template <typename T>
    void set_arg(cl::Kernel& kernel, cl_uint index, T&& arg) {
      try {
        kernel.setArg(index, std::forward<T>(arg));
      } catch (const cl::Error& e) {
        UFW_ERROR("Failed to set {}-th kernel argument: {} ", index, e.what());
      }
    }

    template <typename... Args>
    void set_args(cl::Kernel& kernel, cl_uint index, Args&&... args) {
      // Base case: recursion ends here
    }

    template <typename T, typename... Rest>
    void set_args(cl::Kernel& kernel, cl_uint index, T&& arg, Rest&&... rest) {
      set_arg(kernel, index, std::forward<T>(arg));
      set_args(kernel, index + 1, std::forward<Rest>(rest)...);
    }

    std::vector<float> get_sensitivity_from_system_matrix(const float*, size_4d);

   private:
    cl::platform& r_platform;
    size_t m_n_devices;
    size_t m_voxels_count;
    size_t m_pixels_count;
    cl::Program m_expectation_program;
    cl::Kernel m_expectation_kernel;
    cl::Program m_maximization_program;
    cl::Kernel m_maximization_kernel;
    cl::Program m_invert_matrix_program;
    cl::Kernel m_invert_matrix_kernel;
    cl::Program m_multiply_matrices_in_place_program;
    cl::Kernel m_multiply_matrices_in_place_kernel;
    cl::Program m_add_matrices_in_place_program;
    cl::Kernel m_add_matrices_in_place_kernel;
    std::vector<cl::buffer> m_sensitivity_matrix_buffers;                       // One per GPU
    std::vector<cl::buffer> m_inverted_sensitivity_matrix_buffers;              // One per GPU
    std::unordered_map<channel_id::link_t, cl::buffer> m_system_matrix_buffers; // One per camera
    std::vector<cl::buffer> m_expectation_buffers;                              // One per GPU
    std::vector<cl::buffer> m_maximization_buffers;                             // One per GPU
    std::vector<cl::buffer> m_previous_amplitude_buffers;                       // One per GPU
    grain::voxel_array<uint8_t> m_fiducial;
    float m_voxel_size;
  };

} // namespace sand::grain

UFW_DECLARE_COMPLEX_DATA(sand::grain::volumereco_cl_manager);
