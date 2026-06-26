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

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>
#include <duneanaobj/StandardRecord/SRShower.h>
#include <duneanaobj/StandardRecord/SRTrack.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include "caf_filler_common.hpp"

namespace sand {

  /// @brief Primary template for CAF structure fillers (specializations only)
  template <typename CAFType>
  struct CAFFiller {
    static_assert(ufw::detail::false_for_assertion<CAFType>{}, "CAFFiller is not specialized for this type");
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
