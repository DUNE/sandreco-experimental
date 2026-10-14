#include <edep_reader/edep_reader.hpp>
#include <genie_reader/genie_reader.hpp>
#include <truth_filler/evtcode_parser.hpp>
#include <truth_filler/truth_filler_details.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/filter.hpp>

namespace sand::filter {

  /**
   * \class sand::filter::primary_nu
   *
   * This filter accepts that have at least one primary:
   *  - with a certain type
   *  - with a certain energy range
   */
  class primary_nu : public ufw::filter {
   public:
    primary_nu() : ufw::filter({}) {}

    void configure(const ufw::config& cfg) override {
      m_emin = cfg.at("Emin");
      m_emax = cfg.at("Emax");
      for (auto pdg : cfg.at("nupdg")) {
        m_nutype_pdgs.insert(int(pdg));
      }
    }

    ufw::filter::response evaluate() override {
      const auto& genie     = instance<sand::genie_reader>();
      const auto& tree      = instance<sand::edep_reader>();
      const auto& primaries = tree.GetChildrenTrajectories();

      auto interaction_ranges = common::filler_details::make_interaction_ranges(primaries);

      auto const n_ixn = genie.events_.size();

      UFW_ASSERT(interaction_ranges.size() == n_ixn, "Mismatch between edep-sim interactions ({}) and GENIE events ({})",
                 interaction_ranges.size(), n_ixn);

      for (std::size_t ixn_idx{}; ixn_idx != interaction_ranges.size(); ++ixn_idx) {
        auto [first_prim_idx, prim_count] = interaction_ranges[ixn_idx];
        auto const& event                 = genie.events_[ixn_idx];
        auto const& stdhep                = genie.stdHeps_[ixn_idx];
        for (int j = 0; j < stdhep.N_; j++) {
          auto pdg = stdhep.Pdg_[j];
          if (stdhep.Status_[j] == 0 && m_nutype_pdgs.count(pdg)) {
            auto e = stdhep.P4_[j].E() * 1000.0; //to MeV
            if (e > m_emin && e < m_emax) {
              UFW_INFO("Got a neutrino {}, of energy {}", pdg, e);
              return pass;
            }
          }
        }
      }
      return block;
    }

   private:
    double m_emin;
    double m_emax;
    std::set<int> m_nutype_pdgs;
  };

} // namespace sand::filter

UFW_REGISTER_FILTER(sand::filter::primary_nu)
UFW_REGISTER_DYNAMIC_FILTER_FACTORY(sand::filter::primary_nu)
