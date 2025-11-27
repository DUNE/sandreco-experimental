
#pragma once

#include "ufw/utils.hpp"
#include <ufw/config.hpp>
#include <ufw/data.hpp>

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 220
#include <CL/cl2.hpp>

#define CL_KERNEL(k) "__kernel " #k;

namespace sand::cl {

  using namespace ::cl;

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

    std::vector<cl::Device>& devices() { return m_devices; }

    cl::Context& context() { return m_context; };

    std::vector<cl::CommandQueue>& queues() { return m_queues; }

   private:
    cl::Platform m_platform;
    cl::Context m_context;
    std::vector<cl::Device> m_devices;
    std::vector<cl::CommandQueue> m_queues;
  };

  using Events = std::vector<cl::Event>;

  inline double elapsed_time(const cl::Event& ev) {
    cl_ulong qstart = 0;
    cl_ulong qend   = 0;
    // C++ API throws on this...
    clGetEventProfilingInfo(ev.get(), CL_PROFILING_COMMAND_START, sizeof(qstart), &qstart, nullptr);
    clGetEventProfilingInfo(ev.get(), CL_PROFILING_COMMAND_END, sizeof(qend), &qend, nullptr);
    return 1e-6 * double(qend - qstart);
  }

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

    /**
     * This smart pointer will wait for the map to be completed before returning a user pointer.
     * It will then wait on unmap before going out of scope.
     * @note Only in the event that you perform queued operatios after mapping, that need to complete before unmapping,
     * you have to wait on these events manually before this pointer goes out of scope.
     */
    class autounmapping_ptr {
     public:
      autounmapping_ptr(cl::Buffer& buf, cl::CommandQueue& q) : r_buf(buf), r_q(q), map_ptr(nullptr) {}
      autounmapping_ptr(const autounmapping_ptr&)             = delete;
      autounmapping_ptr& operator= (const autounmapping_ptr&) = delete;
      autounmapping_ptr(autounmapping_ptr&&)                  = default;
      ~autounmapping_ptr() {
        if (map_ptr)
          unmap();
      };

      void* get() {
        map_evt.wait();
        return map_ptr;
      };

      operator void*() { return get(); };

      cl::Event& event() { return map_evt; }

      cl::Event unmap() {
        UFW_ASSERT(map_ptr != nullptr, "unmap called multiple times");
        map_evt.wait();
        cl::Event evt;
        r_q.enqueueUnmapMemObject(r_buf, map_ptr, nullptr, &evt);
        map_ptr = nullptr;
        return std::move(evt);
      }

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
    std::enable_if_t<read_from_host_flags(Flags), void> allocate(const cl::Context& ctx, size_t nbytes, const void* p) {
      UFW_ASSERT(!is_allocated(), "Cannot reallocate buffer.");
      m_buffer = cl::Buffer(ctx, Flags, nbytes, const_cast<void*>(p));
      m_size   = nbytes;
    }

    /**
     * This function allocates a buffer that needs to be initialized from the host via memory mapping.
     * As this is a queued operation, it requires a queue. Will use host pinned memory if available.
     * @returns A pseudo-pointer that can be written to by the user, it will perform the unmap when going out of scope.
     */
    template <cl_mem_flags Flags>
    std::enable_if_t<!read_from_host_flags(Flags), autounmapping_ptr>
    allocate(const cl::Context& ctx, cl::CommandQueue& q, size_t nbytes, const Events& prereq = Events{}) {
      UFW_ASSERT(!is_allocated(), "Cannot reallocate buffer.");
      m_buffer            = cl::Buffer(ctx, Flags | CL_MEM_ALLOC_HOST_PTR, nbytes);
      m_size              = nbytes;
      const Events* evptr = prereq.empty() ? nullptr : &prereq;
      autounmapping_ptr ret(m_buffer, q);
      ret.map_ptr = q.enqueueMapBuffer(m_buffer, false, CL_MAP_WRITE_INVALIDATE_REGION, 0, nbytes, evptr, &ret.map_evt);
      return ret;
    }

    /**
     * This function maps (a subset of) the buffer to host memory for access.
     * Will use pinned memory if the buffer was allocated with CL_MEM_ALLOC_HOST_PTR
     * @returns A pseudo-pointer that can be read from and/or written to by the user, it will perform the unmap when
     * going out of scope.
     */
    template <cl_map_flags Flags = CL_MAP_READ | CL_MAP_WRITE>
    autounmapping_ptr map(cl::CommandQueue& q, size_t offset = 0, size_t sz = -1ul, const Events& prereq = Events{}) {
      UFW_ASSERT(is_allocated(), "Buffer is null.");
      if (sz == -1ul) {
        sz = m_size - offset;
      }
      UFW_ASSERT(sz + offset <= m_size, "Attempted to map() {} bytes into a buffer of {} bytes.", sz + offset, m_size);
      const Events* evptr = prereq.empty() ? nullptr : &prereq;
      autounmapping_ptr ret(m_buffer, q);
      ret.map_ptr = q.enqueueMapBuffer(m_buffer, false, Flags, offset, sz, evptr, &ret.map_evt);
      return ret;
    }

    /**
     * This function reads (a subset of) the buffer with a blocking operation.
     * If the template parameter @p T is a pointer type, this function will allocate the appropriate amount of host
     * memory via the array version of operator new[], read the required data into this and return the pointer.
     * If it is a class with a data() member that returns a pointer, an instance of that class will be default
     * constructed, the data will be read into the pointer returned by data() and this class will be returned.
     * If @p T is neither of those things, then an instance of T will be default constructed, the buffer will be read in
     * place into the instance, and it will be returned.
     */
    template <typename T>
    T read(cl::CommandQueue& q, size_t offset = 0, size_t sz = -1ul, const Events& prereq = Events{}) {
      UFW_ASSERT(is_allocated(), "Buffer is null.");
      if (sz == -1ul) {
        sz = m_size - offset;
      }
      UFW_ASSERT(sz + offset <= m_size, "Attempted to read() {} bytes from a buffer of {} bytes.", sz + offset, m_size);
      const Events* evptr = prereq.empty() ? nullptr : &prereq;
      if constexpr (std::is_pointer_v<T>) {
        using ValT = std::remove_pointer_t<T>;
        T ptr      = new ValT[(sz + sizeof(ValT) - 1) / sizeof(ValT)]; // integer round up
        q.enqueueReadBuffer(m_buffer, CL_TRUE, offset, sz, ptr, evptr);
        return ptr;
      } else if constexpr (std::is_convertible_v<decltype(T{}.data()), void*>) {
        T ret;
        q.enqueueReadBuffer(m_buffer, CL_TRUE, offset, sz, ret.data(), evptr);
        return std::move(ret);
      } else {
        static_assert(sizeof(T) >= sz, "Buffer is too large.");
        T ret;
        q.enqueueReadBuffer(m_buffer, CL_TRUE, offset, sz, &ret, evptr);
        return std::move(ret);
      }
    }

    /**
     * This function reads (a subset of) the buffer with a non-blocking operation.
     * If the template parameter @p T is a pointer type, this function will copy memory into the area pointed to.
     * If it is a class with a data() member that returns a non const pointer, the data will be read into the pointer
     * returned by data(). If @p T is neither of those things, then the buffer will be read directly into it.
     * It is the caller's responsibility to ensure sufficient space in the destination.
     * @returns the Event signaling the end of the operation.
     */
    template <typename T>
    cl::Event read(T& readinto, cl::CommandQueue& q, size_t offset = 0, size_t sz = -1ul,
                   const Events& prereq = Events{}) {
      UFW_ASSERT(is_allocated(), "Buffer is null.");
      if (sz == -1ul) {
        sz = m_size - offset;
      }
      UFW_ASSERT(sz + offset <= m_size, "Attempted to read() {} bytes from a buffer of {} bytes.", sz + offset, m_size);
      cl::Event done;
      const Events* evptr = prereq.empty() ? nullptr : &prereq;
      if constexpr (std::is_pointer_v<T>) {
        q.enqueueReadBuffer(m_buffer, CL_FALSE, offset, sz, readinto, evptr, &done);
      } else if constexpr (std::is_pointer_v<decltype(T{}.data())>) {
        q.enqueueReadBuffer(m_buffer, CL_FALSE, offset, sz, readinto.data(), evptr, &done);
      } else {
        static_assert(sizeof(T) >= sz, "Buffer is too large.");
        q.enqueueReadBuffer(m_buffer, CL_FALSE, offset, sz, &readinto, evptr, &done);
      }
      return std::move(done);
    }

    /**
     * This function writes into (a subset of) the buffer with a non-blocking operation.
     * If the template parameter @p T is a pointer type, this function will copy from the memory pointed to.
     * If it is a class with a data() member that returns a (const) pointer, the data will be read from the pointer
     * returned by data(). If @p T is neither of those things, then it will be copied directly.
     * It is the caller's responsibility to ensure @p readfrom is appropriately sized.
     * @returns the Event signaling the end of the operation.
     */
    template <typename T>
    cl::Event write(const T& readfrom, cl::CommandQueue& q, size_t offset = 0, size_t sz = -1ul,
                    const Events& prereq = Events{}) {
      UFW_ASSERT(is_allocated(), "Buffer is null.");
      if (sz == -1ul) {
        sz = m_size - offset;
      }
      UFW_ASSERT(sz + offset <= m_size, "Attempted to write() {} bytes to a buffer of {} bytes.", sz + offset, m_size);
      const Events* evptr = prereq.empty() ? nullptr : &prereq;
      cl::Event done;
      if constexpr (std::is_pointer_v<T>) {
        q.enqueueWriteBuffer(m_buffer, CL_FALSE, offset, sz, readfrom, evptr, &done);
      } else if constexpr (std::is_convertible_v<decltype(T{}.data()), void*>) {
        q.enqueueWriteBuffer(m_buffer, CL_FALSE, offset, sz, readfrom.data(), evptr, &done);
      } else {
        static_assert(sizeof(T) <= sz, "Buffer is too small.");
        q.enqueueWriteBuffer(m_buffer, CL_FALSE, offset, sz, &readfrom, evptr, &done);
      }
      return std::move(done);
    }

    cl::Event copyto(const buffer& to, cl::CommandQueue& q, size_t offset_this = 0, size_t offset_that = 0,
                     size_t sz = -1ul, const Events& prereq = Events{}) {
      UFW_ASSERT(is_allocated(), "Buffer is null.");
      if (sz == -1ul) {
        sz = std::min(m_size - offset_this, to.m_size - offset_that);
      }
      UFW_ASSERT(sz + offset_this <= m_size, "Attempted to read() {} bytes from a buffer of {} bytes.",
                 sz + offset_this, m_size);
      UFW_ASSERT(sz + offset_that <= to.m_size, "Attempted to write() {} bytes to a buffer of {} bytes.",
                 sz + offset_that, to.m_size);
      const Events* evptr = prereq.empty() ? nullptr : &prereq;
      cl::Event done;
      q.enqueueCopyBuffer(m_buffer, to.m_buffer, offset_this, offset_that, sz, evptr, &done);
      return std::move(done);
    }

    bool is_allocated() const { return m_buffer.get() != nullptr; }

    size_t size() const { return m_size; }

    const cl::Buffer& buf() const { return m_buffer; }

   private:
    cl::Buffer m_buffer;
    size_t m_size = 0;
  };

} // namespace sand::cl

namespace cl::detail {
  // Specialization for our version of buffer
  template <>
  struct KernelArgumentHandler<sand::cl::buffer, void> {
    static size_type size(const sand::cl::buffer&) { return sizeof(cl_mem); }
    static const cl_mem* ptr(const sand::cl::buffer& buf) { return &(buf.buf()()); }
  };

} // namespace cl::detail

UFW_DECLARE_COMPLEX_DATA(sand::cl::platform);

UFW_DECLARE_MANAGED_DATA(sand::cl::buffer);
