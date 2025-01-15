
#include <TObject.h>

#include <config.hpp>
#include <TObjectWrapper.hpp>

namespace sand {

  namespace common {

    TObjectWrapper::~TObjectWrapper() = default;

    TObjectWrapper::TObjectWrapper() = default;

    ufw::data_ptr TObjectWrapper::clone() const {
      auto cl = new TObjectWrapper;
      cl->setObject(m_object->Clone());
      return std::shared_ptr<TObjectWrapper>(cl);
    }

    void TObjectWrapper::configure(const ufw::config& cfg) {
      m_objname = cfg.value("name", "");
    }

    void* TObjectWrapper::get() {
      return m_object.get();
    }

    const void* TObjectWrapper::get() const {
      return m_object.get();
    }

    void TObjectWrapper::setObject(TObject* tobj) {
      assert(tobj);
      m_object.reset(tobj);
      m_objname = m_object->GetName();
    }

  }

}
