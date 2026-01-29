#ifndef SANDRECO_FAKE_RECO_CAF_FILLER_HPP
#define SANDRECO_FAKE_RECO_CAF_FILLER_HPP

/// @file caf_filler.hpp
/// @brief Template-based CAF structure fillers for fake_reco
///
/// Usage:
/// @code
///   auto true_part = CAFFiller<caf::SRTrueParticle>::from_edep(traj, ixn_id);
///   auto track = CAFFiller<caf::SRTrack>::from_true(true_part, id);
/// @endcode

#include "caf_filler_common.hpp"

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPTree.h>
#include <genie_reader/GenieWrapper.h>

#include <vector>

namespace sand {

  /// @brief Primary template for CAF structure fillers (specializations only)
  template <typename CAFType>
  struct CAFFiller {
    static_assert(ufw::detail::false_for_assertion<CAFType>{}, "CAFFiller is not specialized for this type");
  };

  /// @brief Fills SRTrueParticle from edep-sim or GENIE data
  template <>
  struct CAFFiller<::caf::SRTrueParticle> {
    CAFFiller() = delete;

    [[nodiscard]] static ::caf::SRTrueParticle from_edep(const EDEPTrajectory& traj, long int interaction_id,
                                                         const ::caf::TrueParticleID& ancestor_id = {});

    [[nodiscard]] static ::caf::SRTrueParticle from_genie(std::size_t index, const StdHep& stdhep,
                                                          long int interaction_id);
  };

  /// @brief Fills SRTrueInteraction from GENIE and edep-sim data
  template <>
  struct CAFFiller<::caf::SRTrueInteraction> {
    CAFFiller() = delete;

    [[nodiscard]] static ::caf::SRTrueInteraction from_genie(const GRooTrackerEvent& event, const StdHep& stdhep);

    /// @return TrueParticleID for each added primary (use as ancestor_id for secondaries)
    static std::vector<::caf::TrueParticleID> add_primaries(::caf::SRTrueInteraction& ixn,
                                                            const std::vector<EDEPTrajectory>& primaries,
                                                            std::size_t first_idx, std::size_t count);

    static void add_secondaries(::caf::SRTrueInteraction& ixn, EDEPTree::const_iterator begin,
                                EDEPTree::const_iterator end, const ::caf::TrueParticleID& ancestor_id);

    static void add_prefsi(::caf::SRTrueInteraction& ixn, const StdHep& stdhep);
  };

  /// @brief Fills SRRecoParticle from truth (fake reconstruction)
  template <>
  struct CAFFiller<::caf::SRRecoParticle> {
    CAFFiller() = delete;

    [[nodiscard]] static ::caf::SRRecoParticle from_true(const ::caf::SRTrueParticle& true_part,
                                                         const ::caf::TrueParticleID& id);
  };

  /// @brief Fills SRTrack from truth (fake reconstruction)
  template <>
  struct CAFFiller<::caf::SRTrack> {
    CAFFiller() = delete;

    [[nodiscard]] static ::caf::SRTrack from_true(const ::caf::SRTrueParticle& true_part,
                                                  const ::caf::TrueParticleID& id);
  };

  /// @brief Fills SRShower from truth (fake reconstruction)
  template <>
  struct CAFFiller<::caf::SRShower> {
    CAFFiller() = delete;

    [[nodiscard]] static ::caf::SRShower from_true(const ::caf::SRTrueParticle& true_part,
                                                   const ::caf::TrueParticleID& id);
  };

  /// @brief Fills SRInteraction from truth (fake reconstruction)
  template <>
  struct CAFFiller<::caf::SRInteraction> {
    CAFFiller() = delete;

    [[nodiscard]] static ::caf::SRInteraction from_true(const ::caf::SRTrueInteraction& true_ixn,
                                                        std::size_t truth_index);
  };

} // namespace sand

#endif // SANDRECO_FAKE_RECO_CAF_FILLER_HPP
