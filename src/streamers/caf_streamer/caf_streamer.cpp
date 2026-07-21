#include "caf_streamer.hpp"

#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>

#include <TFile.h>
#include <TTree.h>

#define UFW_IMPLEMENT_STREAMER_FOR_TYPE(type) UFW_DECLARE_RTTI(type)
#include <caf_streamer_types.hpp>
#undef UFW_IMPLEMENT_STREAMER_FOR_TYPE

namespace sand::caf {

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

  void caf_streamer::configure(const ufw::config& cfg, ufw::op_type op) {
    streamer::configure(cfg, op);

    const char* open_mode = to_root_open_mode(op);
    if (open_mode == nullptr) {
      UFW_ERROR("Mode {} is not supported by caf_streamer", op);
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

  void caf_streamer::prepare(const ufw::public_id& id, const ufw::type_id& type) {
    streamer::prepare(id, type);
    if (type != ufw::type_of<truth_branch_wrapper>() && type != ufw::type_of<common_reco_branch_wrapper>()
        && type != ufw::type_of<nd_reco_branch_wrapper>()) {
      UFW_ERROR("caf_streamer requires a standard_record_wrapper, truth_branch_wrapper, "
                "common_reco_branch_wrapper or nd_reco_branch_wrapper, got type: {}",
                ufw::simplified_name(type));
    }
  }

  void caf_streamer::attach(ufw::data::data_base& data, const ufw::public_id& id) {
    streamer::attach(data, id);

    if (m_tree->GetBranch(s_data_branch) != nullptr) {
      return;
    }

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
      data_branch->SetAddress(&m_caf_ptr);
    } else if (operation() == ufw::op_type::wo) {
      id_branch   = m_tree->Branch(s_context_id_branch, &m_context_id);
      data_branch = m_tree->Branch(s_data_branch, "caf::StandardRecord", &m_caf_ptr);
    } else {
      UFW_FATAL("ufw::streamer::operation() returned an invalid value");
    }

    if (id_branch != nullptr) {
      id_branch->SetAutoDelete(false);
    }
    data_branch->SetAutoDelete(false);
  }

  void caf_streamer::read(ufw::context_id id) {
    // Required to avoid crash at program exit (ROOT internal state)
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
      // Standard CAF file: use id directly as entry index
      m_tree->GetEntry(static_cast<long>(id));
    }
  }

  void caf_streamer::write(ufw::context_id id) {
    m_file->cd();

    for (auto const& [pub_id, var] : info_map()) {
      if (var.type == ufw::type_of<truth_branch_wrapper>()) {
        m_caf_ptr->mc = *static_cast<truth_branch_wrapper const*>(var.address);
      } else if (var.type == ufw::type_of<common_reco_branch_wrapper>()) {
        m_caf_ptr->common = *static_cast<common_reco_branch_wrapper const*>(var.address);
      } else if (var.type == ufw::type_of<nd_reco_branch_wrapper>()) {
        m_caf_ptr->nd = *static_cast<nd_reco_branch_wrapper const*>(var.address);
      }
    }

    m_context_id = id;
    m_tree->Fill();
    ++m_last_entry;
  }

} // namespace sand::caf

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::caf::caf_streamer)
