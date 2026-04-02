#include "debug_streamer.hpp"

#include <TFile.h>
#include <TTree.h>

#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>

#define UFW_IMPLEMENT_STREAMER_FOR_TYPE(type) UFW_DECLARE_RTTI(type)
#include "debug_streamer_types.hpp"
#undef UFW_IMPLEMENT_STREAMER_FOR_TYPE

namespace sand::debug {

  namespace {
    const char* to_root_open_mode(ufw::op_type op) {
      switch (op) {
      case ufw::op_type::ro:
        return "READ";
      case ufw::op_type::wo:
        return "RECREATE";
      case ufw::op_type::rw:
        return "UPDATE";
      default:
        return nullptr;
      }
    }
  } // namespace

  debug_streamer::~debug_streamer() {
    if (m_file == nullptr) {
      return;
    }
    if (operation() & ufw::op_type::wo) {
      m_file->cd();
      m_tree->Write(nullptr, TObject::kOverwrite);
    }
    m_file->Close();
  }

  void debug_streamer::configure(const ufw::config& cfg, ufw::op_type op) {
    streamer::configure(cfg, op);

    const char* open_mode = to_root_open_mode(op);
    if (open_mode == nullptr) {
      UFW_ERROR("Mode {} is not supported by debug_streamer", op);
    }

    m_file = std::make_unique<TFile>(path().c_str(), open_mode);
    if (!m_file->IsOpen() || m_file->IsZombie()) {
      UFW_ERROR("File {} could not be opened, already open or zombie", path().string());
    }

    const std::string& tree_name = cfg.at("tree");
    if (op & ufw::op_type::ro) {
      m_tree = m_file->Get<TTree>(tree_name.c_str());
    } else {
      m_tree = new TTree(tree_name.c_str(), "");
    }
  }

  void debug_streamer::attach(ufw::data::data_base& data, const ufw::public_id& id) {
    streamer::attach(data, id);

    m_data = static_cast<debug_data*>(&data);

    TBranch* id_branch   = nullptr;
    TBranch* data_branch = nullptr;

    if (operation() & ufw::op_type::ro) {
      id_branch = m_tree->GetBranch(s_context_id_branch);
      if (id_branch != nullptr) {
        m_has_context_id = true;
        id_branch->SetAddress(&m_context_id);
        id_branch->SetAutoDelete(false);
      }

      data_branch = m_tree->GetBranch(s_data_branch);
      if (data_branch == nullptr) {
        UFW_ERROR("TBranch '{}' not found.", s_data_branch);
      }
      data_branch->SetAddress(&m_data);
    } else if (operation() == ufw::op_type::wo) {
      id_branch   = m_tree->Branch(s_context_id_branch, &m_context_id);
      data_branch = m_tree->Branch(s_data_branch, "sand::debug::debug_data", &m_data);
    } else {
      UFW_FATAL("ufw::streamer::operation() returned an invalid value");
    }

    if (id_branch != nullptr) {
      id_branch->SetAutoDelete(false);
    }
    data_branch->SetAutoDelete(false);
  }

  void debug_streamer::read(ufw::context_id id) {
    m_file->cd();

    if (m_has_context_id) {
      if (m_context_id == id) {
        return;
      }
      const long entries = m_tree->GetEntries();
      for (; m_last_entry < entries; ++m_last_entry) {
        m_tree->GetEntry(m_last_entry);
        if (m_context_id == id) {
          return;
        }
      }
      UFW_ERROR("Context id '{}' not found.", id);
    } else {
      m_tree->GetEntry(static_cast<long>(id));
    }
  }

  void debug_streamer::write(ufw::context_id id) {
    m_file->cd();
    m_context_id = id;
    m_tree->Fill();
    ++m_last_entry;
  }

} // namespace sand::debug

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::debug::debug_streamer)
