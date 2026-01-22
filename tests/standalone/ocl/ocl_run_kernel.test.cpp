
#define BOOST_TEST_MODULE ocl

#include <chrono>
#include <random>

#include <boost/test/included/unit_test.hpp>

#include <test_helpers.hpp>

#include <data/common/ocl/ocl.hpp>

BOOST_AUTO_TEST_CASE(run_kernel) {
  constexpr size_t s_max_platforms = 4;
  cl::Program m_program;
  cl::Kernel m_kernel;
  size_t m_array_size;
  const size_t m_local_size = 256;
  size_t m_global_size;
  m_array_size  = 25000000;
  m_global_size = std::ceil(m_array_size / float(m_local_size)) * m_local_size;
  UFW_INFO("Summing two arrays with size : {} MB", m_array_size * sizeof(float) / uint(1 << 20));
  const char* kernel_src =
#include "test_kernel.cl"
      ;

  ufw::config cfg = ufw::json::parse(R"({ "accept_fallback" : true })");
  sand::cl::platform platform(cfg);
  BOOST_TEST(platform.devices().size() > 0);
  m_program = cl::Program(platform.context(), kernel_src);
  m_program.build(platform.devices());
  m_kernel = cl::Kernel(m_program, "vector_add");
  // allocate host memory and create input data
  std::unique_ptr<float[]> A(new float[m_array_size]);
  std::unique_ptr<float[]> B(new float[m_array_size]);
  std::unique_ptr<float[]> C_gpu(new float[m_array_size]);
  std::unique_ptr<float[]> C_cpu(new float[m_array_size]);
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  std::mt19937 random_engine;
  for (size_t i = 0; i < m_array_size; i++) {
    A[i] = dist(random_engine);
    B[i] = dist(random_engine);
  }

  // sequential vector addition on host for comparison
  auto t2 = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < m_array_size; i++) {
    C_cpu[i] = A[i] + B[i];
  }
  auto t3       = std::chrono::high_resolution_clock::now();
  double cpu_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

  UFW_INFO("Completed reference comparison.");

  auto t0 = std::chrono::high_resolution_clock::now();
  // create device buffers.
  sand::cl::buffer bufA; // buf A is initialized by copying from host
  auto t4 = std::chrono::high_resolution_clock::now();
  bufA.allocate<CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY>(platform.context(), m_array_size * sizeof(float), A.get());
  auto t5 = std::chrono::high_resolution_clock::now();
  sand::cl::buffer bufB; // buf A is initialized by pinning from host
  auto ptr = bufB.allocate<CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY>(platform.context(), platform.queues().front(),
                                                                     m_array_size * sizeof(float));

  std::memcpy(ptr, B.get(), m_array_size * sizeof(float));
  auto t6                        = std::chrono::high_resolution_clock::now();
  double copy_buf_A_to_device_ms = std::chrono::duration<double, std::milli>(t5 - t4).count();
  double copy_buf_B_to_device_ms = std::chrono::duration<double, std::milli>(t6 - t5).count();
  auto map_buf_B_ms              = sand::cl::elapsed_time(ptr.event());
  auto unmapevt                  = ptr.unmap();
  auto unmap_buf_B_ms            = sand::cl::elapsed_time(unmapevt);

  sand::cl::buffer bufC;
  bufC.allocate<CL_MEM_WRITE_ONLY>(platform.context(), m_array_size * sizeof(float));

  sand::cl::buffer bufD;
  bufD.allocate<CL_MEM_WRITE_ONLY>(platform.context(), m_array_size * sizeof(float));

  // set kernel args
  m_kernel.setArg(0, bufA);
  m_kernel.setArg(1, bufB);
  m_kernel.setArg(2, bufC);
  m_kernel.setArg(3, m_array_size);
  // execute the kernel
  sand::cl::Events after{unmapevt};
  cl::Event ev_kernel_execution1;
  platform.queues().front().enqueueNDRangeKernel(m_kernel, cl::NullRange, cl::NDRange(m_global_size),
                                                 cl::NDRange(m_local_size), &after, &ev_kernel_execution1);

  m_kernel.setArg(2, bufD);
  // execute the kernel
  cl::Event ev_kernel_execution2;
  platform.queues().front().enqueueNDRangeKernel(m_kernel, cl::NullRange, cl::NDRange(m_global_size),
                                                 cl::NDRange(m_local_size), &after, &ev_kernel_execution2);

  void* wptr                      = C_gpu.get();
  cl::Event ev_copy_C_from_device = bufC.read(wptr, platform.queues().front(), 0, -1, {ev_kernel_execution1});

  auto t7     = std::chrono::high_resolution_clock::now();
  auto dptr   = bufD.map<CL_MAP_READ>(platform.queues().front(), 0, -1, {ev_kernel_execution2});
  float dummy = std::accumulate(static_cast<float*>(dptr.get()), static_cast<float*>(dptr.get()) + m_array_size, 0.f);
  auto t8     = std::chrono::high_resolution_clock::now();
  auto map_buf_D_ms            = sand::cl::elapsed_time(dptr.event());
  double read_buf_D_to_host_ms = std::chrono::duration<double, std::milli>(t8 - t7).count();
  ev_copy_C_from_device.wait();
  auto t1 = std::chrono::high_resolution_clock::now();
  UFW_INFO("Copied results from device to host.");
  // some profiling
  double gpu_wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

  auto gpu_kernel1_ms = sand::cl::elapsed_time(ev_kernel_execution1);
  auto gpu_kernel2_ms = sand::cl::elapsed_time(ev_kernel_execution2);
  auto gpu_read_C_ms  = sand::cl::elapsed_time(ev_copy_C_from_device);

  UFW_INFO("CPU serial time: {} ms", cpu_ms);
  UFW_INFO("GPU wall time (copy to gpu -> enqueue -> finish -> copy result): {} ms", gpu_wall_ms);
  UFW_INFO("Copy array A to GPU (user time): {} ms", copy_buf_A_to_device_ms);
  UFW_INFO("Map array B (from profiling): {} ms", map_buf_B_ms);
  UFW_INFO("Unmap array B (from profiling): {} ms", unmap_buf_B_ms);
  UFW_INFO("Copy array B to GPU (user time): {} ms", copy_buf_B_to_device_ms);
  UFW_INFO("GPU kernel 1 time (from profiling): {} ms", gpu_kernel1_ms);
  UFW_INFO("GPU kernel 2 time (from profiling): {} ms", gpu_kernel2_ms);
  UFW_INFO("Copy result C from GPU (from profiling): {} ms", gpu_read_C_ms);
  UFW_INFO("Map array D (from profiling): {} ms", map_buf_D_ms);
  UFW_INFO("Copy array D from GPU (user time): {} ms", read_buf_D_to_host_ms);

  // validate result (allow tiny FP error)
  float max_abs_err = 0.f;
  for (size_t i = 0; i < m_array_size; i++) {
    max_abs_err = std::max(max_abs_err, std::abs(C_cpu[i] - C_gpu[i]));
  }
  UFW_INFO("Max absolute error: {}", max_abs_err);
  BOOST_TEST(max_abs_err < 1e-7f);
}

FIX_TEST_EXIT
