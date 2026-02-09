
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
    auto& digi        = set<sand::ecal::pes>("pes");

    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap(); // pointer, not value
      if (!trj.HasHitInDetector(component::ECAL))
        continue;

      for (const auto& hit : hit_map.at(component::DRIFT)) {
        auto phit = 0.5 * (hit.GetStart() + hit.GetStop());
        pos_3d h_pos(phit.X(), phit.Y(), phit.Z());
        auto h_t   = phit.T();
        auto h_de  = hit.GetEnergyDeposit();
        auto& cell = gecal.at(h_pos);
        auto l1    = cell.pathlength(h_pos, sand::geoinfo::ecal_info::face_location::begin);
        auto l2    = cell.pathlength(h_pos, sand::geoinfo::ecal_info::face_location::end);
        auto at1   = cell.attenuation(l1);
        auto at2   = cell.attenuation(l2);
        auto nph1  = de_to_nphotons(h_de, at1);
        auto nph2  = de_to_nphotons(h_de, at2);
      }
    }
  }
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