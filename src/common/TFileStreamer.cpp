
#include "TObjectWrapper.hpp"
#include <TFile.h>

#include <config.hpp>
#include <data.hpp>
#include <factory.hpp>

#include <TFileStreamer.hpp>
#include <TObjectWrapper.hpp>
#include <TTreeData.hpp>

namespace sand {

  namespace common {

    TFileStreamer::TFileStreamer() = default;

    TFileStreamer::~TFileStreamer() = default;

    void TFileStreamer::configure(const ufw::config& cfg) {
      streamer::configure(cfg);
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
      return m_mode;
    }

    void TFileStreamer::read(ufw::data& d) {
      TObjectWrapper* tow = dynamic_cast<TObjectWrapper*>(&d);
      if (!tow)
        throw std::runtime_error("Object of type " + d.id() + " is not a TObject and is not supported by TFileStreamer.");
      TObject* obj = m_file->Get(tow->name().c_str());
      if (!obj)
        throw std::runtime_error("TObject " + tow->name() + " not found in file.");
      tow->setObject(obj);
    }

    void TFileStreamer::write(const ufw::data& d) {
      const TObjectWrapper* tow = dynamic_cast<const TObjectWrapper*>(&d);
      if (!tow)
        throw std::runtime_error("Object of type " + d.id() + " is not a TObject and is not supported by TFileStreamer.");
      TFile* current = TFile::CurrentFile();
      if (current != m_file.get())
        m_file->cd();
      auto maybe_tree = dynamic_cast<TTreeDataBase*>(const_cast<TObjectWrapper*>(tow)); //FIXME get rid of this ugly hack
      if (maybe_tree)
        maybe_tree->flush();
      tow->object()->Write();
      if (current != m_file.get())
        current->cd();
    }

  }

}

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::common::TFileStreamer)
