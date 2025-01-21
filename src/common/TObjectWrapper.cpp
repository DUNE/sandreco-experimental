
#include <TObject.h>

#include <config.hpp>
#include <TObjectWrapper.hpp>

namespace sand {

  namespace common {

    TObjectWrapper::~TObjectWrapper() = default;

    TObjectWrapper::TObjectWrapper() {
      if (m_owning)
        delete m_object;
    }

    ufw::data_ptr TObjectWrapper::clone() const {
      auto cl = new TObjectWrapper;
      cl->setObject(m_object->Clone(), true);
      return std::shared_ptr<TObjectWrapper>(cl);
    }

    void TObjectWrapper::configure(const ufw::config& cfg) {
      m_objname = cfg.value("name", "");
    }

    void* TObjectWrapper::get() {
      return m_object;
    }

    const void* TObjectWrapper::get() const {
      return m_object;
    }

    void TObjectWrapper::setObject(TObject* tobj, bool own) {
      assert(tobj);
      if (m_owning)
        delete m_object;
      m_object = tobj;
      m_owning = own;
      m_objname = m_object->GetName();
    }

  }

}
