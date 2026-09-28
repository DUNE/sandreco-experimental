#include "caf_rntuple_streamer.hpp"

#include <caf/caf_wrapper.hpp>

#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>

#include <ROOT/RNTupleModel.hxx>

namespace sand::caf {

  caf_rntuple_streamer::~caf_rntuple_streamer() {
    m_writer.reset();
    m_read_entry.reset();
    m_reader.reset();
  }

  void caf_rntuple_streamer::configure(const ufw::config& cfg, ufw::op_type op) {
    streamer::configure(cfg, op);

    const std::string ntuple_name = cfg.value("ntuple", std::string{"cafTree"});

    if (op == ufw::op_type::wo) {
      auto model   = ROOT::RNTupleModel::Create();
      m_context_id = model->MakeField<std::uint64_t>(s_context_id_field);
      m_record     = model->MakeField<::caf::StandardRecord>(s_data_field);
      m_writer     = ROOT::RNTupleWriter::Recreate(std::move(model), ntuple_name, path().string());
    } else if (op == ufw::op_type::ro) {
      m_reader = ROOT::RNTupleReader::Open(ntuple_name, path().string());

      m_record     = std::make_shared<::caf::StandardRecord>();
      m_context_id = std::make_shared<std::uint64_t>();
      m_read_entry = m_reader->GetModel().CreateBareEntry();
      m_read_entry->BindValue(s_data_field, m_record);

      m_has_context_id = m_reader->GetDescriptor().FindFieldId(s_context_id_field) != ROOT::kInvalidDescriptorId;
      if (m_has_context_id) {
        m_read_entry->BindValue(s_context_id_field, m_context_id);
      }
    } else {
      UFW_ERROR("Mode {} is not supported by caf_rntuple_streamer", op);
    }
  }

  void caf_rntuple_streamer::prepare(const ufw::public_id& id, const ufw::type_id& type) {
    streamer::prepare(id, type);
    if (type != ufw::type_of<truth_branch_wrapper>() && type != ufw::type_of<common_reco_branch_wrapper>()
        && type != ufw::type_of<nd_reco_branch_wrapper>()) {
      UFW_ERROR("caf_rntuple_streamer requires a truth_branch_wrapper, common_reco_branch_wrapper "
                "or nd_reco_branch_wrapper, got type: {}",
                ufw::simplified_name(type));
    }
  }

  void caf_rntuple_streamer::load_entry(std::uint64_t entry) {
    m_reader->LoadEntry(entry, *m_read_entry);
    m_last_entry = entry;
  }

  void caf_rntuple_streamer::read(ufw::context_id id) {
    const std::uint64_t entries = m_reader->GetNEntries();

    if (m_has_context_id) {
      // Scan starting from the last loaded entry, since contexts are normally read in order
      const std::uint64_t start = m_last_entry;
      bool found{false};
      for (std::uint64_t n = 0; n != entries && !found; ++n) {
        load_entry((start + n) % entries);
        found = *m_context_id == static_cast<std::uint64_t>(id);
      }
      if (!found) {
        UFW_ERROR("Context id '{}' not found.", id);
      }
    } else {
      // Standard CAF file: use id directly as entry index
      if (static_cast<std::uint64_t>(id) >= entries) {
        UFW_ERROR("Context id '{}' is out of range ({} entries).", id, entries);
      }
      load_entry(id);
    }

    for (auto const& [pub_id, var] : info_map()) {
      if (var.type == ufw::type_of<truth_branch_wrapper>()) {
        static_cast<::caf::SRTruthBranch&>(*static_cast<truth_branch_wrapper*>(var.address)) = m_record->mc;
      } else if (var.type == ufw::type_of<common_reco_branch_wrapper>()) {
        static_cast<::caf::SRCommonRecoBranch&>(*static_cast<common_reco_branch_wrapper*>(var.address)) =
            m_record->common;
      } else if (var.type == ufw::type_of<nd_reco_branch_wrapper>()) {
        static_cast<::caf::SRNDBranch&>(*static_cast<nd_reco_branch_wrapper*>(var.address)) = m_record->nd;
      }
    }
  }

  void caf_rntuple_streamer::write(ufw::context_id id) {
    for (auto const& [pub_id, var] : info_map()) {
      if (var.type == ufw::type_of<truth_branch_wrapper>()) {
        m_record->mc = *static_cast<truth_branch_wrapper const*>(var.address);
      } else if (var.type == ufw::type_of<common_reco_branch_wrapper>()) {
        m_record->common = *static_cast<common_reco_branch_wrapper const*>(var.address);
      } else if (var.type == ufw::type_of<nd_reco_branch_wrapper>()) {
        m_record->nd = *static_cast<nd_reco_branch_wrapper const*>(var.address);
      }
    }

    *m_context_id = id;
    m_writer->Fill();
  }

} // namespace sand::caf

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::caf::caf_rntuple_streamer)
