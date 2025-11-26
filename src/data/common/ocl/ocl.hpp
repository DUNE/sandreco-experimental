
#pragma once

#include <ufw/config.hpp>
#include <ufw/data.hpp>

#define CL_HPP_ENABLE_EXCEPTIONS
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

  using Events = std::vector<cl::Event>;

  /**
   * It can be useful to pass data between different CL based processes without actually getting this data off the GPU
   * memory: this class provides a named buffer that can be passed around as a variable in the configuration.
   *
   * For efficiency reasons, this is implemented as a global_tag rather than a context_tag, the difference being that
   * this does not get erased across multiple context executions. It also needs to be allocated in device memory before
   * it is used the first time. This needs to be done in the configure() of the first process that uses it.
   *
   * Since CL buffers naturally store large, dense N-dimensional arrays of (typically) floating point values, they are
   * naturally paired with HDF5 files for I/O.
   * For data that only needs to be read once, no streamer is needed, just pass the HD5 file name.
   * An HDF5 streamer will be provided that can perform per context I/O.
   * Note that the streamer will need the buffer to be already allocated when reading (and of the correct size).
   */
  class buffer : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::global_tag> {
   private:
    static constexpr bool read_from_host_flags(cl_mem_flags fl) {
      return (fl & CL_MEM_COPY_HOST_PTR) || (fl & CL_MEM_USE_HOST_PTR);
    }

    class autounmapping_ptr {
     public:
      autounmapping_ptr(cl::Buffer& buf, cl::CommandQueue& q) : r_buf(buf), r_q(q), map_ptr(nullptr) {}
      autounmapping_ptr(const autounmapping_ptr&)             = delete;
      autounmapping_ptr& operator= (const autounmapping_ptr&) = delete;
      autounmapping_ptr(autounmapping_ptr&&)                  = default;
      ~autounmapping_ptr() {
        cl::Event evt;
        r_q.enqueueUnmapMemObject(r_buf, get(), nullptr, &evt);
        evt.wait();
      };

      void* get() {
        map_evt.wait();
        return map_ptr;
      };

      operator void*() { return get(); };

      cl::Event& event() { return map_evt; }

     private:
      cl::Buffer& r_buf;
      cl::CommandQueue& r_q;
      friend class buffer;
      cl::Event map_evt;
      void* map_ptr;
    };

   public:
    buffer()  = default;
    ~buffer() = default;

    /** This function allocates a buffer that does not need to be initialized from the host in any way. */
    template <cl_mem_flags Flags>
    std::enable_if_t<!read_from_host_flags(Flags), void> allocate(const cl::Context& ctx, size_t nbytes) {
      UFW_ASSERT(!is_allocated(), "Cannot reallocate buffer.");
      m_buffer = cl::Buffer(ctx, Flags, nbytes);
      m_size   = nbytes;
    }

    /**
     * This function allocates a buffer that needs to be initialized from the host via a raw pointer to existing memory.
     */
    template <cl_mem_flags Flags>
    std::enable_if_t<read_from_host_flags(Flags), cl::Event> allocate(const cl::Context& ctx, size_t nbytes,
                                                                      const void* p) {
      UFW_ASSERT(!is_allocated(), "Cannot reallocate buffer.");
      m_buffer = cl::Buffer(ctx, Flags, nbytes, p);
      m_size   = nbytes;
    }

    /**
     * This function allocates a buffer that needs to be initialized from the host via memory mapping.
     * As this is a queued operation, it requires a queue. Will use host pinned memory if available.
     * @returns A pseudo-pointer that can be written to by the user, it will perform the unmap when going out of scope.
     */
    template <cl_mem_flags Flags>
    std::enable_if_t<read_from_host_flags(Flags), autounmapping_ptr> allocate(const cl::Context& ctx,
                                                                              cl::CommandQueue& q, size_t nbytes) {
      UFW_ASSERT(!is_allocated(), "Cannot reallocate buffer.");
      m_buffer = cl::Buffer(ctx, Flags | CL_MEM_ALLOC_HOST_PTR, nbytes);
      m_size   = nbytes;
      autounmapping_ptr ret(m_buffer, q);
      ret.map_ptr =
          q.enqueueMapBuffer(m_buffer, false, CL_MAP_WRITE_INVALIDATE_REGION, 0, nbytes, nullptr, &ret.map_evt);
      return ret;
    }

    /**
     * This function maps (a subset of) the buffer to host memory for access.
     * Will use pinned memory if the buffer was allocated with CL_MEM_ALLOC_HOST_PTR
     * @returns A pseudo-pointer that can be written to by the user, it will perform the unmap when going out of scope.
     */
    autounmapping_ptr map(cl::CommandQueue& q, cl_map_flags fl = CL_MAP_READ | CL_MAP_WRITE, size_t offset = 0,
                          size_t sz = -1ul) {
      autounmapping_ptr ret(m_buffer, q);
      if (sz == -1ul) {
        sz = m_size;
      }
      ret.map_ptr = q.enqueueMapBuffer(m_buffer, false, fl, offset, sz, nullptr, &ret.map_evt);
      return ret;
    }

    bool is_allocated() const { return m_buffer.get() != nullptr; }

   private:
    cl::Buffer m_buffer;
    size_t m_size;
  };

} // namespace sand::ocl

UFW_DECLARE_COMPLEX_DATA(sand::ocl::platform);

UFW_DECLARE_MANAGED_DATA(sand::ocl::buffer);
