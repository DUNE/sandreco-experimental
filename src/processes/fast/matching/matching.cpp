#include "matching.hpp"

#include <caf/caf_wrapper.hpp>

#include <duneanaobj/StandardRecord/SREnums.h>
#include <duneanaobj/StandardRecord/SRSAND.h>

#include <ufw/factory.hpp>

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <vector>

namespace sand::fast {

  namespace {

    /// A SAND subdetector object, tagged with the true particle it comes from
    struct TaggedObject {
      ::caf::TrueParticleID particle;
      ::caf::SRSANDObjID object;
    };

    /// Interaction first, then primaries before secondaries, then position in the collection
    bool particle_less(::caf::TrueParticleID const& a, ::caf::TrueParticleID const& b) {
      return std::tie(a.ixn, a.type, a.part) < std::tie(b.ixn, b.type, b.part);
    }

    bool same_particle(::caf::TrueParticleID const& a, ::caf::TrueParticleID const& b) {
      return a.ixn == b.ixn && a.type == b.type && a.part == b.part;
    }
  } // namespace

  matching::matching()
    : process{{{"in_grain", "sand::caf::grain_reco_wrapper"},
               {"in_tracker", "sand::caf::tracker_reco_wrapper"},
               {"in_ecal", "sand::caf::ecal_reco_wrapper"}},
              {{"out_assns", "sand::caf::sand_assn_wrapper"}}} {}

  void matching::run() {
    auto const& grain   = get<sand::caf::grain_reco_wrapper>("in_grain");
    auto const& tracker = get<sand::caf::tracker_reco_wrapper>("in_tracker");
    auto const& ecal    = get<sand::caf::ecal_reco_wrapper>("in_ecal");
    auto& assns         = set<sand::caf::sand_assn_wrapper>("out_assns");

    // 1. Tag every subdetector object with its true particle, in scan order: GRAIN, the tracker, then the ECAL (not
    //    necessarily the crossing order)
    std::vector<TaggedObject> tags;
    tags.reserve(grain.tracks.size() + grain.showers.size() + tracker.tracks.size() + tracker.showers.size()
                 + ecal.clusters.size());
    auto tag_all = [&tags](auto const& objects, ::caf::SRRecoBaseID::SRRecoBaseCollectionType type) {
      for (std::size_t k{}; k != objects.size(); ++k) {
        tags.push_back({objects[k].truth.at(0), {type, static_cast<int>(k)}});
      }
    };
    tag_all(grain.tracks, ::caf::SRRecoBaseID::kSANDGRAINTrack);
    tag_all(grain.showers, ::caf::SRRecoBaseID::kSANDGRAINShower);
    tag_all(tracker.tracks, ::caf::SRRecoBaseID::kSANDTrackerTrack);
    tag_all(tracker.showers, ::caf::SRRecoBaseID::kSANDTrackerShower);
    tag_all(ecal.clusters, ::caf::SRRecoBaseID::kSANDECalCluster);

    // 2. Sort by particle, so that the objects of the same particle become adjacent
    std::stable_sort(tags.begin(), tags.end(),
                     [](auto const& a, auto const& b) { return particle_less(a.particle, b.particle); });

    // 3. Every run of adjacent objects of the same particle is one assn; trk.truth carries the particle
    for (auto const& [particle, object] : tags) {
      if (assns.empty() || !same_particle(assns.back().trk.truth.at(0), particle)) {
        auto& assn            = assns.emplace_back();
        assn.trk.truth        = {particle};
        assn.trk.truthOverlap = {1.f};
      }
      assns.back().constituents.push_back(object);
    }
  }

} // namespace sand::fast

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fast::matching);
