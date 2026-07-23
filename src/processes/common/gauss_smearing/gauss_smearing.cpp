#include "gauss_smearing.hpp"

#include <caf/caf_wrapper.hpp>

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRRecoParticle.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>

#include <cmath>
#include <cstddef>
#include <random>

namespace sand::common {

  namespace {
    float smear_component(float v, double sigma) {
      auto& rng = ufw::context::current()->engine();
      return v + static_cast<float>(std::normal_distribution<double>(0.0, sigma)(rng));
    }

    ::caf::SRVector3D smear_position(::caf::SRVector3D const& pos, double x_res, double y_res, double z_res) {
      return ::caf::SRVector3D{smear_component(pos.x, x_res), smear_component(pos.y, y_res),
                               smear_component(pos.z, z_res)};
    }

    float smear_energy(float energy, double energy_res) {
      double const sigma = energy_res * std::sqrt(energy);
      return smear_component(energy, sigma);
    }

    ::caf::SRVector3D smear_momentum(::caf::SRVector3D const& p, double momentum_res) {
      float const p_mag        = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
      float const p_transverse = std::hypot(p.y, p.z);
      float const dip_angle    = std::atan(p.x / p_transverse);
      float const zy_angle     = std::atan2(p.y, p.z);

      double const sigma  = momentum_res * p_mag;
      float const smeared = smear_component(p_mag, sigma);

      return ::caf::SRVector3D{smeared * std::sin(dip_angle), smeared * std::cos(dip_angle) * std::sin(zy_angle),
                               smeared * std::cos(dip_angle) * std::cos(zy_angle)};
    }
  } // namespace

  ::caf::SRInteraction smear_interaction(::caf::SRInteraction const& reco_ixn, double energy_res, double x_res,
                                         double y_res, double z_res) {
    ::caf::SRInteraction smeared = reco_ixn;

    smeared.vtx = smear_position(reco_ixn.vtx, x_res, y_res, z_res);

    float const smeared_e       = smear_energy(reco_ixn.Enu.calo, energy_res);
    smeared.Enu.calo            = smeared_e;
    smeared.Enu.lep_calo        = smeared_e;
    smeared.Enu.mu_range        = smeared_e;
    smeared.Enu.mu_mcs          = smeared_e;
    smeared.Enu.mu_mcs_llhd     = smeared_e;
    smeared.Enu.e_calo          = smeared_e;
    smeared.Enu.e_had           = smeared_e;
    smeared.Enu.mu_had          = smeared_e;
    smeared.Enu.regcnn          = smeared_e;
    smeared.Enu.part_energy_sum = smeared_e;

    return smeared;
  }

  ::caf::SRRecoParticle smear_particle(::caf::SRRecoParticle const& reco_part, double energy_res, double momentum_res,
                                       double x_res, double y_res, double z_res) {
    ::caf::SRRecoParticle smeared = reco_part;

    smeared.E     = smear_energy(reco_part.E, energy_res);
    smeared.p     = smear_momentum(reco_part.p, momentum_res);
    smeared.start = smear_position(reco_part.start, x_res, y_res, z_res);
    smeared.end   = smear_position(reco_part.end, x_res, y_res, z_res);

    return smeared;
  }

  gauss_smearing::gauss_smearing()
    : process{{{"in_common", "sand::caf::common_reco_branch_wrapper"}},
              {{"out_common", "sand::caf::common_reco_branch_wrapper"}}} {}

  void gauss_smearing::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_energy_res   = cfg.value("energy_resolution", 0.);
    m_momentum_res = cfg.value("momentum_resolution", 0.);
    m_x_res        = cfg.value("x_resolution", 0.);
    m_y_res        = cfg.value("y_resolution", 0.);
    m_z_res        = cfg.value("z_resolution", 0.);
  }

  void gauss_smearing::run() {
    auto const& in_common = get<sand::caf::common_reco_branch_wrapper>("in_common");
    auto& out_common      = set<sand::caf::common_reco_branch_wrapper>("out_common");

    out_common.ixn.sandreco.reserve(in_common.ixn.sandreco.size());

    for (auto const& reco_ixn : in_common.ixn.sandreco) {
      auto& smeared_ixn =
          out_common.ixn.sandreco.emplace_back(smear_interaction(reco_ixn, m_energy_res, m_x_res, m_y_res, m_z_res));

      for (std::size_t i{}; i != reco_ixn.part.sandreco.size(); ++i) {
        smeared_ixn.part.sandreco[i] =
            smear_particle(reco_ixn.part.sandreco[i], m_energy_res, m_momentum_res, m_x_res, m_y_res, m_z_res);
      }
    }

    out_common.ixn.nsandreco = in_common.ixn.nsandreco;
  }

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::gauss_smearing);
