#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#define CL_TARGET_OPENCL_VERSION 220
#include <CL/cl.h>
#include <random>
#include <chrono>


namespace sand::common {

    class opencl_test : public ufw::process {

    public:
      opencl_test();
      void configure (const ufw::config& cfg) override;
      void run() override;
  
    private:
      void create_device();
      void create_ctx_queue();
      void print_device_info();
      void build_kernel();
      void cleanup();

    private:
      cl_platform_id m_platform;
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
    
    void opencl_test::configure (const ufw::config& cfg) {
      process::configure(cfg);
      m_rng_engine.seed(cfg.at("seed"));
      m_array_size = cfg.at("array_size"); 
      m_global_size = ceil(m_array_size / (float)m_local_size) * m_local_size; 
      UFW_INFO("Summing two arrays with size : {} MB", m_array_size * sizeof(float) / int(1 << 20));
    }
  
    opencl_test::opencl_test() : process({}, {}) {
      UFW_DEBUG("Creating a opencl_test process at {}.", fmt::ptr(this));
    }
  
    void opencl_test::run() {
      // GPU setup
      cl_int err;
      create_device();
      print_device_info();
      create_ctx_queue();
      build_kernel();
      
      // allocate host memory and create input data
      float *A, *B, *C_gpu, *C_cpu; 
      A = (float*)malloc(sizeof(float)*m_array_size);
      B = (float*)malloc(sizeof(float)*m_array_size);
      C_cpu = (float*)malloc(sizeof(float)*m_array_size);
      C_gpu = (float*)malloc(sizeof(float)*m_array_size); 
      std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
      for (size_t i = 0; i < m_array_size; i++){
        A[i] = dist(m_rng_engine);
        B[i] = dist(m_rng_engine);
      }
      
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
      }
      else 
        UFW_DEBUG("Kernel arguments set.");
      
      // copy buffers to device 
      auto t0 = std::chrono::high_resolution_clock::now();
      err = clEnqueueWriteBuffer(m_queue, bufA, CL_FALSE, 0, sizeof(float) * m_array_size, A, 0, NULL, NULL);  
      err |= clEnqueueWriteBuffer(m_queue, bufB, CL_FALSE, 0, sizeof(float) * m_array_size, B, 0, NULL, NULL);  
      if (err != CL_SUCCESS)
        UFW_ERROR("Failed to copy buffers.");
      else
        UFW_DEBUG("Copied buffers to device.");
      
      // execute the kernel
      cl_event ev;
      err = clEnqueueNDRangeKernel(m_queue, m_kernel, 1, NULL, &m_global_size, &m_local_size, 0, NULL, &ev);
      if (err != CL_SUCCESS)
        UFW_ERROR("Failed to enqueue kernel.");
      else
        UFW_DEBUG("Kernel enqueued.");
      
      clWaitForEvents(1, &ev);
      clEnqueueReadBuffer(m_queue, bufC, CL_TRUE, 0, m_array_size * sizeof(float), C_gpu, 0, NULL, NULL );
      UFW_DEBUG("Copied results from device to host.");
      
      auto t1 = std::chrono::high_resolution_clock::now();
      double gpu_wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      cl_ulong qstart = 0, qend = 0;
      clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_START, sizeof(qstart), &qstart, nullptr);
      clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_END, sizeof(qend), &qend, nullptr);
      double gpu_kernel_ms = 1e-6 * double(qend - qstart); // ns -> ms

      // sequential vector addition on host for comparison
      auto t2 = std::chrono::high_resolution_clock::now();
      for (size_t i = 0; i < m_array_size; i++){
        C_cpu[i] = A[i] + B[i]; 
      } 
      auto t3 = std::chrono::high_resolution_clock::now();
      double cpu_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

      UFW_INFO("CPU serial time: {} ms", cpu_ms);
      UFW_INFO("GPU wall time (copy to gpu -> enqueue -> finish -> copy result): {} ms", gpu_wall_ms);
      UFW_INFO("GPU kernel time (from profiling): {} ms", gpu_kernel_ms);

      // validate result (allow tiny FP error)
      double max_abs_err = 0.0;
      for (size_t i = 0; i < m_array_size; i++){
        double e = std::abs(double(C_cpu[i]) - double(C_gpu[i]));
        if (e > max_abs_err)
          max_abs_err = e; 
      }
      if (max_abs_err > 1e-6)
        UFW_INFO("Results between sequential sum on host and device differ!");
      else
        UFW_INFO("Results between sequential sum on host and device match.");
      UFW_DEBUG("Max absolute error: {}", max_abs_err);
    }
  
    void opencl_test::create_device() {       
      cl_int err;
      err = clGetPlatformIDs(1, &m_platform, NULL);
      if (err < 0) 
        UFW_ERROR("Could not identify a platform.");
      // access a device, look for a GPU first
      err = clGetDeviceIDs(m_platform, CL_DEVICE_TYPE_GPU, 1, &m_device, NULL);
      if (err == CL_DEVICE_NOT_FOUND)   // switch to CPU
        err = clGetDeviceIDs(m_platform, CL_DEVICE_TYPE_CPU, 1, &m_device, NULL);
      if (err < 0) 
        UFW_ERROR("Could not access any devices.");
      else 
        UFW_INFO("Device found");
    }

    void opencl_test::create_ctx_queue(){
      cl_int err;
      m_context = clCreateContext(NULL, 1, &m_device, NULL, NULL, &err);
      if (err != CL_SUCCESS)
        UFW_ERROR("Could not create a context.");
      // enable time profiling in queue: openCL API > 2.0 wants null-terminated properties list
      const cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};  
      m_queue = clCreateCommandQueueWithProperties(m_context, m_device, props, &err);
      if (err != CL_SUCCESS)
        UFW_ERROR("Could not create a command queue.");
      else
        UFW_DEBUG("Context and queue created.");
    }

    void opencl_test::print_device_info(){
      auto getStr = [](cl_device_id d, cl_device_info param) -> std::string {
        size_t sz = 0;
        clGetDeviceInfo(d, param, 0, nullptr, &sz);
        std::string s(sz, '\0');
        clGetDeviceInfo(d, param, sz, &s[0], nullptr);
        if (!s.empty() && s.back() == '\0') s.pop_back();
        return s;
      };  
      
      std::string name = getStr(m_device, CL_DEVICE_NAME);
      std::string vendor = getStr(m_device, CL_DEVICE_VENDOR);
      std::string drv = getStr(m_device, CL_DRIVER_VERSION);
      UFW_INFO("Device: {} {}, driver version: {}", vendor, name, drv);
      
      cl_uint cu;
      clGetDeviceInfo(m_device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cu), &cu, nullptr);
      size_t wg;
      clGetDeviceInfo(m_device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(wg), &wg, nullptr);
      cl_ulong mem;
      clGetDeviceInfo(m_device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(mem), &mem, nullptr);
      UFW_INFO("Device compute units: {}, Max work-group size: {}, Global memory (bytes): {}", cu, wg, mem);      
    }

    void opencl_test::build_kernel(){
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
      }
      else
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
      }
      else
        UFW_DEBUG("Built program without errors.");

      m_kernel = clCreateKernel(m_program, "vector_add", &err);
      if (err != CL_SUCCESS){
        UFW_ERROR("Failed to create kernel.");
        cleanup();
      }
      else
        UFW_DEBUG("Created kernel vector_add");
    }

    void opencl_test::cleanup(){
        clReleaseProgram(m_program); 
        clReleaseCommandQueue(m_queue); 
        clReleaseContext(m_context);
    }
  }
  
  UFW_REGISTER_PROCESS(sand::common::opencl_test)
  UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::opencl_test)
  