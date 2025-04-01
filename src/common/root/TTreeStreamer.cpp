
#include <TFile.h>
#include <TTree.h>

#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>

#include <TFileWrapper.hpp>
#include <TTreeStreamer.hpp>

#define UFW_IMPLEMENT_STREAMER_FOR_TYPE(type) \
UFW_DECLARE_RTTI(type)
#include <TTreeStreamerTypes.hpp>
#undef UFW_IMPLEMENT_STREAMER_FOR_TYPE

namespace sand::common::root {

  TTreeStreamer::~TTreeStreamer() {
    m_tree->Write();
    delete m_tree;
    delete m_file;
  }

  void TTreeStreamer::configure(const ufw::config& cfg) {
    streamer::configure(cfg);
    std::string openmode = cfg.value("mode", "READ");
    if (openmode == "READ") {
      m_mode = iop::ro;
    } else if (openmode == "RECREATE" || openmode == "CREATE" || openmode == "NEW") {
      m_mode = iop::wo;
    } else if (openmode == "UPDATE") {
      m_mode = iop::rw;
    } else {
      throw std::runtime_error("Mode " + openmode + " is not supported by TTreeStreamer");
    }
    m_file = new TFileWrapper(path().c_str(), openmode.c_str());
    if (!m_file->IsOpen() || m_file->IsZombie()) {
      throw std::runtime_error("File " + path().string() + " could not be opened");
    }
    std::string treename = cfg.at("tree");
    if (m_mode == iop::ro || m_mode == iop::rw) {
      m_tree = m_file->Get<TTree>(treename.c_str());
    } else {
      m_tree = new TTree(treename.c_str(), "");
    }
  }

  void TTreeStreamer::attach(const ufw::type_id& t, ufw::data::data_base& d) {
    TClass* tcl = TClass::GetClass(t.c_str());
    assert(tcl);
    //you would think using a temporary here would be fine, and yet...
    m_branchaddr = &d;
    if (m_mode == iop::ro || m_mode == iop::rw) {
      TBranch* br = m_tree->GetBranch(t.c_str());
      assert(br);
      br->SetAddress(&m_branchaddr);
    } else if (m_mode == iop::wo) {
      m_tree->Branch(t.c_str(), t.c_str(), &m_branchaddr);
    }
  }

  ufw::streamer::iop TTreeStreamer::support(const ufw::type_id& t) const {
    if (ufw::data::is_complex_type(t) || ufw::data::is_global_type(t)) {
      return iop::none;
    }
    TClass* tcl = TClass::GetClass(t.c_str());
    return tcl ? m_mode : iop::none;
  }

  void TTreeStreamer::read(ufw::context_id i) {
    m_tree->GetEntry(i);
  }

  void TTreeStreamer::write(ufw::context_id) {
    m_tree->Fill();
  }

}

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::common::root::TTreeStreamer)
