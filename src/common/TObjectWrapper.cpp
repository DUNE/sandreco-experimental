
#include <TObject.h>

#include <config.hpp>
#include <TObjectWrapper.hpp>

namespace sand {

  namespace common {

    TObjectWrapper::~TObjectWrapper() = default;

    TObjectWrapper::TObjectWrapper(TObject* tobj)  : m_object(tobj) {
      if (m_object)
        m_objname = m_object->GetName();
    }

    ufw::data_ptr TObjectWrapper::clone() const {
      return std::shared_ptr<TObjectWrapper>(new TObjectWrapper(m_object->Clone()));
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
      m_object.reset(tobj);
      m_objname = m_object->GetName();
    }

  }

}
