
#pragma once

#include <ufw/config.hpp>
#include <ufw/data.hpp>

#define CL_HPP_TARGET_OPENCL_VERSION 220
#include <CL/cl2.hpp>

namespace sand::ocl {

  /**
   * This class queries OpenCL for all possible platforms. It selects the one with the most GPU devices.
   * If the "accept_fallback" (default false) option is true, it will accept the platfrom with the most CPU devices in
   * case it does not find any GPUs.
   *
   * Then, it creates a context with at most "max_gpus" (default infinity) in it, and a queue for each device.
   *
   * As construction will fail if no valid devices are found, you are guaranteed to have a valid context and at least
   * one device and queue.
   */
  class platform : ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {
   public:
    platform(const ufw::config&);

    virtual ~platform() = default;

    const std::vector<cl::Device>& devices() { return m_devices; }

    const cl::Context& context() { return m_context; };

    const std::vector<cl::CommandQueue>& queues() { return m_queues; }

   private:
    cl::Platform m_platform;
    cl::Context m_context;
    std::vector<cl::Device> m_devices;
    std::vector<cl::CommandQueue> m_queues;
  };

} // namespace sand::ocl

UFW_DECLARE_COMPLEX_DATA(sand::ocl::platform);
