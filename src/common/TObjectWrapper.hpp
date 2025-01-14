#pragma once

#include <data.hpp>

class TObject;

namespace sand {

  namespace common {

    class TObjectWrapper : public ufw::data {

    public:
      TObjectWrapper(TObject* tobj = nullptr);

      ~TObjectWrapper() override;

      ufw::data_ptr clone() const override;

      void configure(const ufw::config&) override;

      void setObject(TObject* tobj);

      TObject* object() { return m_object.get(); }

      const TObject* object() const { return m_object.get(); }

      const std::string& name() const { return m_objname; }

    protected:
      void* get() override;

      const void* get() const override;

    private:
      std::unique_ptr<TObject> m_object;
      std::string m_objname;

    };

  }

}
