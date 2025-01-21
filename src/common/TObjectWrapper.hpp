#pragma once

#include <data.hpp>

class TObject;

namespace sand {

  namespace common {

    class TObjectWrapper : public ufw::data {

    public:
      TObjectWrapper();

      ~TObjectWrapper() override;

      ufw::data_ptr clone() const override;

      void configure(const ufw::config&) override;

      virtual void setObject(TObject* tobj, bool own);

      TObject* object() { return m_object; }

      const TObject* object() const { return m_object; }

      const std::string& name() const { return m_objname; }

    protected:
      void* get() override;

      const void* get() const override;

    private:
      TObject* m_object = nullptr;
      bool m_owning = true;
      std::string m_objname;

    };

  }

}
