#include <ocl.hpp>

namespace sand::cl {

  using namespace ::cl;

  platform::platform(const ufw::config& cfg) {
    bool accept_fallback = cfg.value("accept_fallback", true);
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.empty()) {
      UFW_ERROR("No CL plaforms found.");
    }
    UFW_INFO("Found {} CL platforms:", platforms.size());
    // print them all
    size_t best_gpu_count = 0;
    size_t best_gpu_index = 0;
    size_t best_cpu_count = 0;
    size_t best_cpu_index = 0;
    for (size_t i = 0; i < platforms.size(); i++) {
      auto& p             = platforms[i];
      std::string name    = p.getInfo<CL_PLATFORM_NAME>();
      std::string vendor  = p.getInfo<CL_PLATFORM_VENDOR>();
      std::string version = p.getInfo<CL_PLATFORM_VERSION>();
      UFW_INFO(" - {} {} version {}", name, vendor, version);
      std::vector<cl::Device> gpus;
      p.getDevices(CL_DEVICE_TYPE_GPU, &gpus); // we seem to have an old API version
      std::vector<cl::Device> cpus;
      p.getDevices(CL_DEVICE_TYPE_CPU, &cpus); // we seem to have an old API version
      UFW_INFO("   with {} GPU devices and {} CPU devices.", gpus.size(), cpus.size());
      if (gpus.size() > best_gpu_count) {
        best_gpu_count = gpus.size();
        best_gpu_index = i;
      }
      if (cpus.size() > best_cpu_count) {
        best_cpu_count = cpus.size();
        best_cpu_index = i;
      }
    }

    if (best_gpu_count) {
      m_platform       = platforms[best_gpu_index];
      std::string name = m_platform.getInfo<CL_PLATFORM_NAME>();
      UFW_INFO("Using platform {} with {} GPU devices.", name, best_gpu_count);
      m_platform.getDevices(CL_DEVICE_TYPE_GPU, &m_devices);
    } else if (accept_fallback && best_cpu_count) {
      m_platform       = platforms[best_cpu_index];
      std::string name = m_platform.getInfo<CL_PLATFORM_NAME>();
      UFW_WARN("No GPU platforms found. Using {} with {} CPU devices as a fallback.", name, best_cpu_count);
      m_platform.getDevices(CL_DEVICE_TYPE_CPU, &m_devices);
    }

    UFW_INFO("Found {} CL devices:", m_devices.size());

    size_t max_gpus = cfg.value("max_gpus", -1ul);
    if (max_gpus < m_devices.size()) {
      UFW_INFO("But will only use {}", max_gpus);
      m_devices.resize(max_gpus);
    }
    for (const auto& dev : m_devices) {
      std::string name   = dev.getInfo<CL_DEVICE_NAME>();
      std::string vendor = dev.getInfo<CL_DEVICE_VENDOR>();
      std::string drv    = dev.getInfo<CL_DRIVER_VERSION>();
      cl_uint cu         = dev.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
      size_t wg          = dev.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
      cl_ulong mem       = dev.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
      UFW_INFO("- {} {}, driver version: {}", vendor, name, drv);
      UFW_INFO("  with {} compute units, max work-group size {}, global memory {} MB", cu, wg, mem / (1 << 20));
    }

    m_context = cl::Context(m_devices);
    for (const auto& dev : m_devices) {
      m_queues.emplace_back(m_context, dev, CL_QUEUE_PROFILING_ENABLE);
    }
  }

  /**
   * Build program and get errors if any
   */
  void platform::build_program(cl::Program& program, const char* kernel_src) {
    try {
      program = cl::Program(context(), kernel_src);
      program.build(devices());
    } catch (const cl::Error& e) {
      UFW_WARN("OpenCL Program Build Error: {} ({})", e.what(), e.err());
      // Fetch logs from each device
      for (const auto& dev : devices()) {
        UFW_WARN("Device: {}", dev.getInfo<CL_DEVICE_NAME>());
        UFW_WARN("{}", program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev));
      }
      UFW_ERROR("Program Build Failed.");
    }
  }
}; // namespace sand::cl
