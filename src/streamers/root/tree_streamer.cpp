
#include <TFile.h>
#include <TTree.h>

#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>

#include <tree_streamer.hpp>

#define UFW_IMPLEMENT_STREAMER_FOR_TYPE(type) UFW_DECLARE_RTTI(type)
#include <tree_streamer_types.hpp>
#undef UFW_IMPLEMENT_STREAMER_FOR_TYPE

namespace sand::root {

  tree_streamer::tree_streamer() : m_file(nullptr), m_tree(nullptr), m_branchaddr(nullptr), m_id(), m_last_entry(-1) {
    m_id_ptr = &m_id;
  }

  tree_streamer::~tree_streamer() {
    if (operation() & ufw::op_type::wo) {
      m_file->cd();
      m_tree->Write(0, TObject::kOverwrite);
    }
    m_file->Close();
    // delete m_tree; //deleted in close
  }

  void tree_streamer::configure(const ufw::config& cfg, const ufw::type_id& tp, ufw::op_type op) {
    streamer::configure(cfg, tp, op);
    TClass* tcl = TClass::GetClass(tp.c_str());
    if (!tcl) {
      UFW_ERROR("TClass for '{}' not found.", tp);
    }
    const char* opmode = "";
    switch (op) {
    case ufw::op_type::ro:
      opmode = "READ";
      break;
    case ufw::op_type::wo:
      opmode = "RECREATE";
      break;
    case ufw::op_type::rw:
      opmode = "UPDATE";
      break;
    default:
      UFW_ERROR("Mode {} is not supported by tree_streamer", op);
      break;
    };
    m_file.reset(new TFile(path().c_str(), opmode));
    if (!m_file->IsOpen() || m_file->IsZombie()) {
      UFW_ERROR("File {} could not be opened", path().string());
    }
    std::string treename = cfg.at("tree");
    if (op & ufw::op_type::ro) {
      m_tree = m_file->Get<TTree>(treename.c_str());
    } else {
      m_tree = new TTree(treename.c_str(), "");
    }
  }

  void tree_streamer::attach(ufw::data::data_base& d) {
    TBranch* brid   = nullptr;
    TBranch* brdata = nullptr;
    m_branchaddr    = &d;
    // remove any namespace from the branch name, for convenience.
    auto brname = ufw::simplified_name(type());
    if (operation() & ufw::op_type::ro) {
      // attach index branch
      brid = m_tree->GetBranch(s_id_brname);
      if (!brid) {
        UFW_ERROR("TBranch '{}' not found.", s_id_brname);
      }
      brid->SetAddress(&m_id_ptr);
      // attach data branch
      brdata = m_tree->GetBranch(brname.c_str());
      if (!brdata) {
        UFW_ERROR("TBranch '{}' not found.", brname);
      }
      brdata->SetAddress(&m_branchaddr);
    } else if (operation() == ufw::op_type::wo) {
      brid   = m_tree->Branch(s_id_brname, &m_id_ptr);
      brdata = m_tree->Branch(brname.c_str(), type().c_str(), &m_branchaddr);
    }
    // unclear if this is default...
    brid->SetAutoDelete(false);
    brdata->SetAutoDelete(false);
    streamer::attach(d);
  }

  void tree_streamer::read(ufw::context_id id) {
    if (m_id == id) {
      return;
    }
    m_file->cd(); // TODO figure out why removing these causes crash at program exit
    long entries = m_tree->GetEntries();
    // linear search
    while (++m_last_entry < entries) {
      m_tree->GetEntry(m_last_entry);
      if (m_id == id) {
        return;
      }
    }
    UFW_ERROR("Context id '{}' not found.", id);
  }

  void tree_streamer::write(ufw::context_id id) {
    m_file->cd();
    m_id = id;
    m_tree->Fill();
    ++m_last_entry;
  }

} // namespace sand::root

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::root::tree_streamer)
