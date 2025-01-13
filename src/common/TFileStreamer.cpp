
#include <TFile.h>

#include <config.hpp>
#include <data.hpp>

#include <TFileStreamer.hpp>

namespace sand {

  namespace common {

    class TObjectWrapper : public ufw::data {

    public:
      TObjectWrapper(TObject* tobj) : m_object(tobj) {}

      ~TObjectWrapper() override;

      ufw::data_ptr clone() const override { return std::shared_ptr<TObjectWrapper>(new TObjectWrapper(m_object->Clone())); }

      void setObject(TObject* tobj) { m_object.reset(tobj); }

      TObject* object() { return m_object.get(); }

      const TObject* object() const { return m_object.get(); }

    protected:
      void* get() override;

      const void* get() const override;

    private:
      std::unique_ptr<TObject> m_object;

    };

    TFileStreamer::~TFileStreamer() = default;

    void TFileStreamer::configure(const ufw::config& cfg) {
      std::string openmode = cfg.value("mode", "READ");
      if (openmode == "READ")
        m_mode = iop::ro;
      else if (openmode == "RECREATE" || openmode == "CREATE" || openmode == "NEW")
        m_mode = iop::wo;
      else if (openmode == "UPDATE")
        m_mode = iop::rw;
      else
        throw std::runtime_error("Mode " + openmode + " is not supported by TFileStreamer");
      m_file.reset(new TFile(path().c_str(), openmode.c_str()));
    }

    TFileStreamer::iop TFileStreamer::support(const ufw::type_id& id) const {
      if (id == ufw::register_data_type_id<TObjectWrapper>::datatype_id)
        return m_mode;
      else
        return iop::none;
    }

    void TFileStreamer::read(ufw::data& d) {

    }

    void TFileStreamer::write(const ufw::data& d) {
      const TObjectWrapper& tow = ufw::data_cast<TObjectWrapper>(d);
      const TObject* tobj = tow.object();
      TFile* current = TFile::CurrentFile();
      if (current != m_file.get())
        m_file->cd();
      tobj->Write();
      if (current != m_file.get())
        current->cd();
    }

  }

}
