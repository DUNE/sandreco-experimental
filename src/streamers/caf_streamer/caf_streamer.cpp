#include <caf_streamer.hpp>

#include <TFile.h>

#include <caf/caf_wrapper.hpp>

#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>

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

  caf_streamer::~caf_streamer() {
    if (file_ == nullptr) {
      return;
    }
    if (operation() & ufw::op_type::wo) {
      file_->cd();
      tree_->Write(nullptr, TObject::kOverwrite);
    }
    file_->Close();
  }

  void caf_streamer::configure(const ufw::config& cfg, ufw::op_type op) {
    streamer::configure(cfg, op);

    const char* open_mode = to_root_open_mode(op);
    if (open_mode == nullptr) {
      UFW_ERROR("Mode {} is not supported by caf_streamer", op);
    }

    file_ = std::make_unique<TFile>(path().c_str(), open_mode);
    if (!file_->IsOpen() || file_->IsZombie()) {
      UFW_ERROR("File {} could not be opened, already open or zombie", path().string());
    }

    const std::string& tree_name = cfg.at("tree");
    if (op & ufw::op_type::ro) {
      tree_ = file_->Get<TTree>(tree_name.c_str());
    } else {
      tree_ = new TTree(tree_name.c_str(), "");
    }
  }

  void caf_streamer::attach(ufw::data::data_base& data, const ufw::public_id& id) {
    streamer::attach(data, id);

    const var_info& info = info_map().at(id);
    data_ptr_            = static_cast<caf_wrapper*>(&data);

    const auto branch_name = ufw::simplified_name(info.type);

    TBranch* id_branch   = nullptr;
    TBranch* data_branch = nullptr;

    if (operation() & ufw::op_type::ro) {
      id_branch = tree_->GetBranch(kContextIdBranchName);
      if (id_branch == nullptr) {
        UFW_ERROR("TBranch '{}' not found.", kContextIdBranchName);
      }
      id_branch->SetAddress(&context_id_);

      data_branch = tree_->GetBranch(branch_name.c_str());
      if (data_branch == nullptr) {
        UFW_ERROR("TBranch '{}' not found.", branch_name);
      }
      data_branch->SetAddress(&data_ptr_);
    } else if (operation() == ufw::op_type::wo) {
      id_branch   = tree_->Branch(kContextIdBranchName, &context_id_);
      data_branch = tree_->Branch(branch_name.c_str(), info.type.c_str(), &data_ptr_);
    } else {
      UFW_FATAL("ufw::streamer::operation() returned an invalid value");
    }

    // Prevent ROOT from auto-deleting branch data on GetEntry()
    id_branch->SetAutoDelete(false);
    data_branch->SetAutoDelete(false);
  }

  void caf_streamer::read(ufw::context_id id) {
    if (context_id_ == id) {
      return;
    }

    // Required to avoid crash at program exit (ROOT internal state)
    file_->cd();

    const long entries = tree_->GetEntries();
    for (; last_entry_ < entries; ++last_entry_) {
      tree_->GetEntry(last_entry_);
      if (context_id_ == id) {
        return;
      }
    }
    UFW_ERROR("Context id '{}' not found.", id);
  }

  void caf_streamer::write(ufw::context_id id) {
    file_->cd();
    context_id_ = id;
    tree_->Fill();
    ++last_entry_;
  }

} // namespace sand::caf

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::caf::caf_streamer)
