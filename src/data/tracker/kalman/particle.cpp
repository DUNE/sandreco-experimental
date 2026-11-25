#include <kalman/particle.hpp>

#include <Geant4/CLHEP/Units/SystemOfUnits.h>
#include <TDatabasePDG.h>
#include <TParticlePDG.h>

#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace sand::tracker::kalman {

  double mass_from_pdg(int pdg) {
    static std::mutex mutex;
    static std::unordered_map<int, double> cache;

    std::lock_guard<std::mutex> lock(mutex);
    auto const it = cache.find(pdg);
    if (it != cache.end()) {
      return it->second;
    }
    auto const* p = TDatabasePDG::Instance()->GetParticle(pdg);
    if (p == nullptr) {
      throw std::invalid_argument("sand::tracker::kalman::mass_from_pdg: unknown PDG code " + std::to_string(pdg));
    }
    double const m = p->Mass() * CLHEP::GeV;
    cache.emplace(pdg, m);
    return m;
  }

} // namespace sand::tracker::kalman
