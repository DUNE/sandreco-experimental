#include <hdf5/hdf5.hpp>
#include <volumereco_cl_manager.hpp>

#include <ufw/context.hpp>

namespace sand::grain {

  volumereco_cl_manager::volumereco_cl_manager(const ufw::config&)
    : r_platform(ufw::context::current()->instance<cl::platform>()) {
    m_n_devices = r_platform.devices().size();
    configure_expectation();
    configure_maximization();
    configure_invert_matrix();
    configure_multiply_matrices_in_place();
    configure_add_matrices_in_place();
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

} // namespace sand::grain
