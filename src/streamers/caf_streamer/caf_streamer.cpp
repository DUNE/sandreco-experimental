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

caf_streamer::caf_streamer() : streamer{} {}

caf_streamer::~caf_streamer() {
    if (operation() & ufw::op_type::wo) {
        file_->cd();
        tree_->Write(nullptr, TObject::kOverwrite);
    }
    file_->Close();
}

void caf_streamer::configure(const ufw::config& cfg, const ufw::type_id& tp,
                             ufw::op_type op) {
    UFW_ASSERT(tp == ufw::type_of<caf_wrapper>(),
               "Trying to handle a {} with caf_streamer",
               ufw::type_of<caf_wrapper>());
    streamer::configure(cfg, tp, op);
    // const TClass* tcl = TClass::GetClass(tp.c_str());
    // if (!tcl) {
    //     UFW_ERROR("TClass for '{}' not found.", tp);
    // }
    auto op_mode = "";
    switch (op) {
    case ufw::op_type::ro:
        op_mode = "READ";
        break;
    case ufw::op_type::wo:
        op_mode = "RECREATE";
        break;
    case ufw::op_type::rw:
        op_mode = "UPDATE";
        break;
    default:
        UFW_ERROR("Mode {} is not supported by caf_streamer", op);
        break;
    }
    file_ = std::make_unique<TFile>(path().c_str(), op_mode);
    if (!file_->IsOpen() || file_->IsZombie()) {
        UFW_ERROR("File {} could not be opened, already open or zombie",
                  path().string());
    }

    const std::string& tree_name = cfg.at("tree");
    if (op & ufw::op_type::ro) {
        tree_.reset(file_->Get<TTree>(tree_name.c_str()));
    } else {
        tree_ = std::make_unique<TTree>(tree_name.c_str(), "");
    }
}

void caf_streamer::attach(ufw::data::data_base& d) {
    TBranch* id_branch = nullptr;
    TBranch* data_branch = nullptr;
    // Do we trust the ufw enough to use a static_cast?
    data_ptr_ = dynamic_cast<caf_wrapper*>(&d);
    // remove any namespace from the branch name, for convenience.
    const auto branch_name = ufw::simplified_name(type());
    if (operation() & ufw::op_type::ro) {
        // attach index branch
        id_branch = tree_->GetBranch(s_id_brname);
        if (!id_branch) {
            UFW_ERROR("TBranch '{}' not found.", s_id_brname);
        }
        id_branch->SetAddress(&context_id_);
        // attach data branch
        data_branch = tree_->GetBranch(branch_name.c_str());
        if (!data_branch) {
            UFW_ERROR("TBranch '{}' not found.", branch_name);
        }
        data_branch->SetAddress(&data_ptr_);
    } else if (operation() == ufw::op_type::wo) {
        id_branch = tree_->Branch(s_id_brname, &context_id_);
        data_branch =
            tree_->Branch(branch_name.c_str(), type().c_str(), &data_ptr_);
    } else {
        UFW_FATAL("ufw::streamer::operation() returned an invalid value");
    }
    // unclear if this is default...
    id_branch->SetAutoDelete(false);
    data_branch->SetAutoDelete(false);
    streamer::attach(d);
}

void caf_streamer::read(ufw::context_id id) {
    if (context_id_ == id) {
        return;
    }
    file_->cd(); // TODO figure out why removing these causes crash at program
                 //  exit (copy pasted from tree_streamer.cpp)
    const long entries = tree_->GetEntries();
    // linear search
    while (++last_entry_ < entries) {
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
