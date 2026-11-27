#include <kalman/particle.hpp>

#include <Geant4/CLHEP/Units/SystemOfUnits.h>
#include <TDatabasePDG.h>
#include <TParticlePDG.h>

#include <cmath>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace sand::tracker::kalman {

  particle_hypothesis hypothesis_from_pdg(int pdg) {
    static std::mutex mutex;
    static std::unordered_map<int, particle_hypothesis> cache;

    std::lock_guard<std::mutex> lock(mutex);
    auto const it = cache.find(pdg);
    if (it != cache.end()) {
      return it->second;
    }
    auto const* p = TDatabasePDG::Instance()->GetParticle(pdg);
    if (p == nullptr) {
      throw std::invalid_argument("sand::tracker::kalman::hypothesis_from_pdg: unknown PDG code "
                                  + std::to_string(pdg));
    }
    // TParticlePDG::Charge() is in units of |e|/3
    particle_hypothesis const h{p->Mass() * CLHEP::GeV, std::abs(p->Charge()) / 3.};
    if (!(h.abs_charge > 0.)) {
      throw std::invalid_argument("sand::tracker::kalman::hypothesis_from_pdg: PDG code " + std::to_string(pdg)
                                  + " is a neutral particle");
    }
    cache.emplace(pdg, h);
    return h;
  }

} // namespace sand::tracker::kalman
