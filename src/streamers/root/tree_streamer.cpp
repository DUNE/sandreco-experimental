
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

  void tree_streamer::configure(const ufw::config& cfg, ufw::op_type op) {
    streamer::configure(cfg, op);
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
    TBranch* brid = nullptr;
    if (operation() & ufw::op_type::ro) {
      // attach index branch
      brid = m_tree->GetBranch(s_id_brname);
      UFW_ASSERT(brid != nullptr, "TBranch '{}' not found.", s_id_brname);
      brid->SetAddress(&m_id_ptr);
    } else if (operation() == ufw::op_type::wo) {
      brid = m_tree->Branch(s_id_brname, &m_id_ptr);
    }
    // unclear if this is default...
    brid->SetAutoDelete(false);
  }

  void tree_streamer::prepare(const ufw::public_id& id, const ufw::type_id& tp) {
    TClass* tcl = TClass::GetClass(tp.c_str());
    UFW_ASSERT(tcl != nullptr, "TClass for '{}' not found: type is not supported.", tp);
    ufw::streamer::prepare(id, tp);
  }

  void tree_streamer::attach(ufw::data::data_base& d, const ufw::public_id& id) {
    streamer::attach(d, id);
    var_info& info  = info_map().at(id);
    info.address    = &d;
    auto brname     = ufw::simplified_name(info.type); // remove any namespace from the branch name, for convenience.
    TBranch* brdata = nullptr;
    if (operation() & ufw::op_type::ro) {
      // attach data branch
      brdata = m_tree->GetBranch(brname.c_str());
      UFW_ASSERT(brdata != nullptr, "TBranch '{}' not found.", brname);
      brdata->SetAddress(&info.address);
    } else if (operation() == ufw::op_type::wo) {
      brdata = m_tree->Branch(brname.c_str(), info.type.c_str(), &info.address);
    }
    // unclear if this is default...
    brdata->SetAutoDelete(false);
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
