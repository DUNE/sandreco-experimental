#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <ocl/ocl.hpp>

#include <chrono>
#include <random>

namespace sand::common {

  class opencl_test : public ufw::process {
   public:
    opencl_test();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    void create_device_ctx_queue();
    void build_kernel();
    void cleanup();
    double time_profile(cl_event ev);

   private:
    static constexpr size_t s_max_platforms = 4;
    cl_device_id m_device;
    cl_context m_context;
    cl_command_queue m_queue;
    cl_program m_program;
    cl_kernel m_kernel;
    std::mt19937 m_rng_engine;
    size_t m_array_size;
    const size_t m_local_size = 256;
    size_t m_global_size;
  };

  void opencl_test::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_rng_engine.seed(cfg.at("seed"));
    m_array_size  = cfg.at("array_size");
    m_global_size = std::ceil(m_array_size / float(m_local_size)) * m_local_size;
    UFW_INFO("Summing two arrays with size : {} MB", m_array_size * sizeof(float) / uint(1 << 20));
    create_device_ctx_queue();
    build_kernel();
  }

  opencl_test::opencl_test() : process({}, {}) { UFW_DEBUG("Creating an opencl_test process at {}.", fmt::ptr(this)); }

  void opencl_test::run() {
    UFW_DEBUG("Running an opencl_test process at {}.", fmt::ptr(this));
    // GPU setup
    cl_int err;

    // allocate host memory and create input data
    std::unique_ptr<float[]> A(new float[m_array_size]);
    std::unique_ptr<float[]> B(new float[m_array_size]);
    std::unique_ptr<float[]> C_cpu(new float[m_array_size]);
    std::unique_ptr<float[]> C_gpu(new float[m_array_size]);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (size_t i = 0; i < m_array_size; i++) {
      A[i] = dist(m_rng_engine);
      B[i] = dist(m_rng_engine);
    }

    // sequential vector addition on host for comparison
    auto t2 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < m_array_size; i++) {
      C_cpu[i] = A[i] + B[i];
    }
    auto t3       = std::chrono::high_resolution_clock::now();
    double cpu_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

    UFW_DEBUG("Completed reference comparison.");

    // create device buffers
    cl_mem bufA = clCreateBuffer(m_context, CL_MEM_READ_ONLY, m_array_size * sizeof(float), NULL, &err);
    cl_mem bufB = clCreateBuffer(m_context, CL_MEM_READ_ONLY, m_array_size * sizeof(float), NULL, &err);
    cl_mem bufC = clCreateBuffer(m_context, CL_MEM_WRITE_ONLY, m_array_size * sizeof(float), NULL, &err);
    if (!bufA || !bufB || !bufC)
      UFW_ERROR("Failed to create buffers.");
    else
      UFW_DEBUG("Buffers created.");

    // set kernel args
    err = clSetKernelArg(m_kernel, 0, sizeof(cl_mem), &bufA);
    err |= clSetKernelArg(m_kernel, 1, sizeof(cl_mem), &bufB);
    err |= clSetKernelArg(m_kernel, 2, sizeof(cl_mem), &bufC);
    err |= clSetKernelArg(m_kernel, 3, sizeof(size_t), &m_array_size);
    if (err != CL_SUCCESS) {
      UFW_ERROR("Failed to set up kernel arguments.");
      cleanup();
    } else
      UFW_DEBUG("Kernel arguments set.");

    // copy buffers to device
    auto t0 = std::chrono::high_resolution_clock::now();
    cl_event ev_writebuf;
    err =
        clEnqueueWriteBuffer(m_queue, bufA, CL_FALSE, 0, sizeof(float) * m_array_size, A.get(), 0, NULL, &ev_writebuf);
    err |= clEnqueueWriteBuffer(m_queue, bufB, CL_FALSE, 0, sizeof(float) * m_array_size, B.get(), 0, NULL, NULL);
    if (err != CL_SUCCESS)
      UFW_ERROR("Failed to copy buffers.");
    else
      UFW_DEBUG("Copied buffers to device.");

    // execute the kernel
    cl_event ev_kernel_execution;
    err = clEnqueueNDRangeKernel(m_queue, m_kernel, 1, NULL, &m_global_size, &m_local_size, 0, NULL,
                                 &ev_kernel_execution);
    if (err != CL_SUCCESS)
      UFW_ERROR("Failed to enqueue kernel.");
    else
      UFW_DEBUG("Kernel enqueued.");

    cl_event ev_copy_from_device;
    clEnqueueReadBuffer(m_queue, bufC, CL_FALSE, 0, m_array_size * sizeof(float), C_gpu.get(), 1, &ev_kernel_execution,
                        &ev_copy_from_device);
    clWaitForEvents(1, &ev_copy_from_device);
    auto t1 = std::chrono::high_resolution_clock::now();

    UFW_DEBUG("Copied results from device to host.");
    // some profiling
    double gpu_wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto gpu_kernel_ms      = time_profile(ev_kernel_execution);
    auto gpu_copy_kernel_ms = time_profile(ev_writebuf);
    auto gpu_copy_result_ms = time_profile(ev_copy_from_device);

    UFW_INFO("CPU serial time: {} ms", cpu_ms);
    UFW_INFO("GPU wall time (copy to gpu -> enqueue -> finish -> copy result): {} ms", gpu_wall_ms);
    UFW_INFO("Copy 1 array to GPU (from profiling): {} ms", gpu_copy_kernel_ms);
    UFW_INFO("GPU kernel time (from profiling): {} ms", gpu_kernel_ms);
    UFW_INFO("Copy result from GPU (from profiling): {} ms", gpu_copy_result_ms);

    // validate result (allow tiny FP error)
    float max_abs_err = 0.f;
    for (size_t i = 0; i < m_array_size; i++) {
      max_abs_err = std::max(max_abs_err, std::abs(C_cpu[i] - C_gpu[i]));
    }
    if (max_abs_err > 1e-6f)
      UFW_INFO("Results between sequential sum on host and device differ!");
    else
      UFW_INFO("Results between sequential sum on host and device match.");
    UFW_DEBUG("Max absolute error: {}", max_abs_err);
  }

  double opencl_test::time_profile(cl_event ev) {
    cl_ulong qstart = 0;
    cl_ulong qend   = 0;
    clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_START, sizeof(qstart), &qstart, nullptr);
    clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_END, sizeof(qend), &qend, nullptr);
    double gpu_ms = 1e-6 * double(qend - qstart); // ns -> ms
    return gpu_ms;
  }

  void opencl_test::create_device_ctx_queue() {
    auto& platform = instance<sand::ocl::platform>();
    m_device       = platform.devices()[0].get();
    m_context      = platform.context().get();
    m_queue        = platform.queues()[0].get();
  }

  void opencl_test::build_kernel() {
    const char* kernel_src =
        "__kernel void vector_add(__global const float* A, __global const float* B, __global float* C, ulong N) {\n"
        "  size_t i = get_global_id(0);\n"
        "  if (i < N) C[i] = A[i] + B[i];\n"
        "}\n";
    cl_int err;
    m_program = clCreateProgramWithSource(m_context, 1, &kernel_src, nullptr, &err);
    if (!m_program || err != CL_SUCCESS) {
      UFW_ERROR("Failed to create program from kernel source.");
      clReleaseCommandQueue(m_queue);
      clReleaseContext(m_context);
    } else
      UFW_DEBUG("Created program from kernel source.");
    err = clBuildProgram(m_program, 1, &m_device, nullptr, nullptr, nullptr);
    if (err != CL_SUCCESS) {
      // print build log
      size_t logsz = 0;
      clGetProgramBuildInfo(m_program, m_device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logsz);
      std::string log(logsz, '\0');
      clGetProgramBuildInfo(m_program, m_device, CL_PROGRAM_BUILD_LOG, logsz, &log[0], nullptr);
      UFW_ERROR("Failed to build program:\n {}", log);
      cleanup();
    } else
      UFW_DEBUG("Built program without errors.");

    m_kernel = clCreateKernel(m_program, "vector_add", &err);
    if (err != CL_SUCCESS) {
      UFW_ERROR("Failed to create kernel.");
      cleanup();
    } else
      UFW_DEBUG("Created kernel vector_add");
  }

  void opencl_test::cleanup() {
    clReleaseProgram(m_program);
    clReleaseCommandQueue(m_queue);
    clReleaseContext(m_context);
  }
} // namespace sand::common

UFW_REGISTER_PROCESS(sand::common::opencl_test)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::opencl_test)
