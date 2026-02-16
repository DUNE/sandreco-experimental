
#include <random>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/ecal_info.hpp>
#include <optical_simulation.hpp>
#include <ecal/pes.h>

namespace sand::ecal {

  void optical_simulation::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_light_yield = cfg.at("light_yield");
  }

  optical_simulation::optical_simulation() : process({}, {{"pes", "sand::ecal::pes"}}) {
    UFW_DEBUG("Creating ECAL optical simulation process at {}", fmt::ptr(this));
  }

  void optical_simulation::run() {
    UFW_DEBUG("Running ECAL optical simulation process at {}", fmt::ptr(this));
    const auto& tree  = get<sand::edep_reader>();
    const auto& gecal = get<geoinfo>().ecal();
    auto& pes         = set<sand::ecal::pes>("pes");

    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap();
      if (!trj.HasHitInDetector(component::ECAL))
        continue;

      sand::geoinfo::ecal_info::cell const* pcell = nullptr;

      for (const auto& hit : hit_map.at(component::ECAL)) {
        auto phit = 0.5 * (hit.GetStart() + hit.GetStop());
        pos_3d h_pos(phit.X(), phit.Y(), phit.Z());
        auto h_t  = phit.T();
        auto h_de = hit.GetEnergyDeposit();
        try {
          auto& cell = gecal.at(h_pos);
          pcell      = &cell;
        } catch (const std::invalid_argument& e) {
          continue;
        }
        auto& fiber = pcell->get_fiber();
        // how to map sand::geoinfo::ecal_info::face_location to channel_id::side_t?
        auto l1   = pcell->pathlength(h_pos, sand::geoinfo::ecal_info::face_location::begin);
        auto l2   = pcell->pathlength(h_pos, sand::geoinfo::ecal_info::face_location::end);
        auto at1  = pcell->attenuation(l1);
        auto at2  = pcell->attenuation(l2);
        auto cid  = pcell->id();
        auto nph1 = de_to_nphotons(h_de, at1);
        auto nph2 = de_to_nphotons(h_de, at2);
        for (int i = 0; i < nph1; i++) {
          pes::pe pe_;
          pe_.arrival_time = h_t + scintillation_time(fiber.scintillation_rise_time, fiber.scintillation_decay_time)
                           + propagation_time(l1, fiber.light_velocity);
          channel_id chid;
          chid.ecal_pmt.subdetector   = sand::subdetector_t::ECAL;
          chid.ecal_pmt.region        = static_cast<sand::channel_id::region_t>(cid.region);
          chid.ecal_pmt.module_number = sand::channel_id::module_t(cid.module_number);
          chid.ecal_pmt.row           = sand::channel_id::row_t(cid.row);
          chid.ecal_pmt.column        = sand::channel_id::column_t(cid.column);
          chid.ecal_pmt.side          = sand::channel_id::side_t::X_PLUS;
          pes.collection[chid].emplace_back(std::move(pe_));
        }
        for (int i = 0; i < nph2; i++) {
          pes::pe pe_;
          pe_.arrival_time = h_t + scintillation_time(fiber.scintillation_rise_time, fiber.scintillation_decay_time)
                           + propagation_time(l2, fiber.light_velocity);
          channel_id chid;
          chid.ecal_pmt.subdetector   = sand::subdetector_t::ECAL;
          chid.ecal_pmt.region        = static_cast<sand::channel_id::region_t>(cid.region);
          chid.ecal_pmt.module_number = sand::channel_id::module_t(cid.module_number);
          chid.ecal_pmt.row           = sand::channel_id::row_t(cid.row);
          chid.ecal_pmt.column        = sand::channel_id::column_t(cid.column);
          chid.ecal_pmt.side          = sand::channel_id::side_t::X_MINUS;
          pes.collection[chid].emplace_back(std::move(pe_));
        }
      }
    }
  }

  int optical_simulation::de_to_nphotons(double de, double attenuation) const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::poisson_distribution<int> poisson(m_light_yield * de * attenuation);
    return poisson(gen);
  };

  double optical_simulation::scintillation_time(double rise_time, double decay_time) const {
    // function reimplemented from here:
    // https://github.com/Geant4/geant4/blob/e58e650b32b961c8093f3dd6a2c3bc917b2552be/source/processes/electromagnetic/xrays/src/G4Scintillation.cc#L638
    double t;
    static std::random_device rd;  // Will be used to obtain a seed for the random number engine
    static std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
    static std::uniform_real_distribution<> rnd_unif(0.0, 1.0);

    do {
      // The exponential distribution as an envelope function: very efficient
      t = -1.0 * decay_time * std::log(1.0 - rnd_unif(gen));
    } while (rnd_unif(gen) > (1.0 - std::exp(-t / rise_time)));

    return t;
  }

} // namespace sand::ecal