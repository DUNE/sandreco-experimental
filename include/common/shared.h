#pragma once

#include <common/sand.h>

#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace sand::reco {

  /**
   * \class sand::reco::shared
   * \brief Base class for objects owned by quota.
   *
   * An object of a class derived from shared represents a physical object (a hit, a cluster, a track, ...)
   * whose total ownership is 1.0. The quota of the object is the fraction of that physical object that
   * this particular instance represents; the quotas of all live instances of the same physical object sum
   * to 1.0.
   *
   * Objects are created via shared::create, which stamps a quota of 1.0. The only operation that duplicates
   * the data of an object is shared::split, which consumes an object and returns two new objects with
   * quotas scaled by the split fraction. An object may be moved at most through its unique_ptr (ownership
   * transfer): both shared and all of its derived children have their copy and move constructors and
   * assignments suppressed, so instances cannot be copied or moved outside of split.
   *
   * A derived class must override the private copy_self() member, which makes a plain copy of itself
   * (e.g. `return new Child(*this);`). A derived class owning other shared objects must additionally
   * provide a deep copy constructor using shared::clone_ptr to re-allocate those objects. Note that a
   * derived class must declare its copy constructor explicitly as protected; the one implicitly declared
   * for it would be public, and a derived class needs its base's copy constructor to be accessible in
   * order to copy it.
   *
   * The default constructor is protected, initializes the quota to NaN, and is only needed by ROOT to
   * deserialize derived classes; it must never be used in code, as it produces an invalid object.
   */
  class shared {
   public:
    /**
     * Create a new shared object of type \p Child, forwarding \p args to the Child constructor.
     * The returned object owns the whole physical object (quota 1.0).
     */
    template <typename Child, typename... Args>
    static std::unique_ptr<Child> create(Args&&... args) {
      static_assert(std::is_base_of_v<shared, Child>, "Child must inherit from sand::reco::shared");
      auto out = std::unique_ptr<Child>(new Child(std::forward<Args>(args)...));
      set_quota(out.get(), 1.0);
      return out;
    }

    /**
     * Split an object into two, consuming \p x (pass by rvalue).
     * Returns two new objects with the data of \p x, with quotas of \p x scaled by \p frac and (1 - \p frac)
     * respectively.
     * \p frac must satisfy 0 < frac < 1.
     */
    template <typename Child>
    static std::pair<std::unique_ptr<Child>, std::unique_ptr<Child>> split(std::unique_ptr<Child>&& x, double frac) {
      static_assert(std::is_base_of_v<shared, Child>, "Child must inherit from sand::reco::shared");
      UFW_ASSERT(frac > 0.0 && frac < 1.0, "shared::split requires 0 < frac < 1, got {}", frac);
      const double q = x->quota();
      auto a        = clone_ptr(x);
      auto b        = clone_ptr(x);
      set_quota(a.get(), q * frac);
      set_quota(b.get(), q * (1.0 - frac));
      return {std::move(a), std::move(b)};
    }

    /**
     * @returns the fraction of the physical object represented by this instance.
     */
    double quota() const { return m_quota; }

    /**
     * @returns true if this instance owns the whole physical object.
     */
    bool is_unique() const { return m_quota == 1.0; }

    virtual ~shared() = default;

   protected:
    shared() : m_quota(std::numeric_limits<double>::quiet_NaN()) {}

    shared(const shared&) = default;

    /**
     * Make a copy of the shared object owned by \p x, returned as a fresh unique_ptr of the same static type.
     * The quota is copied as-is.
     * Only shared::split and copy constructors of derived classes may use this.
     */
    template <typename Child>
    static std::unique_ptr<Child> clone_ptr(const std::unique_ptr<Child>& x) {
      return std::unique_ptr<Child>(static_cast<Child*>(do_copy(x.get())));
    }

   private:
    shared(shared&&) = delete;
    shared& operator=(const shared&) = delete;
    shared& operator=(shared&&) = delete;

    /**
     * Make a plain copy of this object, allocated with new.
     * Must be overridden by derived classes.
     */
    virtual shared* copy_self() const = 0;

    /**
     * Call copy_self on a shared object.
     * This non-template indirection is deliberate: the call to copy_self made here is checked in the
     * context of shared, which is required for clone_ptr (a template) to be callable from outside.
     */
    static shared* do_copy(const shared* x) { return x->copy_self(); }

    static void set_quota(shared* p, double q) { p->m_quota = q; }

    double m_quota;
  };

} // namespace sand::reco

UFW_DECLARE_UNMANAGED_DATA(sand::reco::shared)
