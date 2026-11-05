#include "truth_matching.hpp"

#include <duneanaobj/StandardRecord/SREnums.h>
#include <duneanaobj/StandardRecord/SRSAND.h>

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <vector>

namespace sand::common {

  namespace {

    /// A SAND subdetector object, tagged with the true particle it comes from
    struct TaggedObject {
      ::caf::TrueParticleID particle;
      ::caf::SRSANDObjID object;
    };

    /// Every subdetector object, tagged with its truth[0], in collection order
    std::vector<TaggedObject> tag_objects(::caf::SRSAND const& objects) {
      std::vector<TaggedObject> tagged;
      tagged.reserve(objects.tracker.tracks.size() + objects.tracker.showers.size());

      auto const tag = [&](auto const& collection, ::caf::SRRecoBaseID::SRRecoBaseCollectionType type) {
        for (std::size_t k{}; k != collection.size(); ++k) {
          tagged.push_back({collection[k].truth.at(0), {type, static_cast<int>(k)}});
        }
      };
      tag(objects.tracker.tracks, ::caf::SRRecoBaseID::kSANDTrackerTrack);
      tag(objects.tracker.showers, ::caf::SRRecoBaseID::kSANDTrackerShower);
      return tagged;
    }

    /// Interaction first, then primaries before secondaries, then position in the collection
    bool particle_less(::caf::TrueParticleID const& a, ::caf::TrueParticleID const& b) {
      return std::tie(a.ixn, a.type, a.part) < std::tie(b.ixn, b.type, b.part);
    }

    bool same_particle(::caf::TrueParticleID const& a, ::caf::TrueParticleID const& b) {
      return a.ixn == b.ixn && a.type == b.type && a.part == b.part;
    }

  } // namespace

  std::vector<::caf::SRSANDAssn> match_by_truth(::caf::SRSAND const& objects) {
    // 1. Tag every subdetector object with its true particle
    auto tagged = tag_objects(objects);

    // 2. Sort by particle, so that the objects of the same particle become adjacent
    std::stable_sort(tagged.begin(), tagged.end(),
                     [](auto const& a, auto const& b) { return particle_less(a.particle, b.particle); });

    // 3. Every run of adjacent objects with the same particle is one assn
    std::vector<::caf::SRSANDAssn> assns;
    for (auto const& [particle, object] : tagged) {
      if (assns.empty() || !same_particle(assns.back().trk.truth.at(0), particle)) {
        auto& assn            = assns.emplace_back();
        assn.trk.truth        = {particle};
        assn.trk.truthOverlap = {1.f};
      }
      assns.back().constituents.push_back(object);
    }
    return assns;
  }

} // namespace sand::common
