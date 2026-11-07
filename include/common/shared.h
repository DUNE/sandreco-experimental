#pragma once

#include <common/data.h>
#include <common/version.h>

namespace sand {

  /**
   * \class sand::shared_object
   *
   * \brief Base class for objects that are shared between records within the same context.
   *
   * Shared objects (hits, vertices, etc. will derive from this in the future) are stored in a
   * per-context store and referenced from records through the typed key @c sand::shared_index,
   * so that the same object can be referenced by any number of records without duplication.
   * The store and the records holding keys into it are streamed as branches of the same TTree
   * entry for the same context, so a key can never dangle on read-back.
   *
   * This class itself carries no data: it only exists to give the store a common element type
   * and a place for derived shared objects to hang their state.
   */
  class shared_object {
   public:
    shared_object() = default;

    virtual ~shared_object() = default;
  };

} // namespace sand

// Per-context store of shared objects, addressed through the typed key sand::shared_index.
// Provides sand::shared (the collection type and the registered variable name), the
// sand::shared_store() accessor and the sand::shared_index key type.
SAND_SHARED_COLLECTION(sand, shared_object, shared)

namespace sand {

  /**
   * \class sand::shared_record
   *
   * \brief Managed record holding a key into the shared-object store.
   *
   * The key streams as a plain index and resolves against the store of the current context.
   */
  class shared_record : public managed_data_base {
   public:
    shared_record() = default;

    explicit shared_record(const shared_index& key) : m_key(key) {}

    const shared_index& key() const { return m_key; }

    void set_key(const shared_index& key) { m_key = key; }

   private:
    shared_index m_key;
  };

} // namespace sand

UFW_DECLARE_MANAGED_DATA(sand::shared_record)
