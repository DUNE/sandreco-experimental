#include <kalman/material.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace sand::tracker::kalman {

  material_budget::~material_budget() = default;

  bool layer_in_interval(double z_layer, double z_from, double z_to, int direction) {
    if (z_from == z_to) {
      return false;
    }
    double const lo = std::min(z_from, z_to);
    double const hi = std::max(z_from, z_to);
    return direction > 0 ? (z_layer > lo && z_layer <= hi) : (z_layer >= lo && z_layer < hi);
  }

  fixed_material_budget::fixed_material_budget(std::vector<layer> layers) : m_layers(std::move(layers)) {
    std::sort(m_layers.begin(), m_layers.end(), [](const layer& a, const layer& b) { return a.z < b.z; });
  }

  std::vector<layer> fixed_material_budget::crossed_layers(const state& s, double z_target) const {
    std::vector<layer> out;
    auto const in = [&](const layer& l) { return layer_in_interval(l.z, s.z, z_target, s.direction); };
    if (z_target >= s.z) {
      std::copy_if(m_layers.begin(), m_layers.end(), std::back_inserter(out), in);
    } else {
      std::copy_if(m_layers.rbegin(), m_layers.rend(), std::back_inserter(out), in);
    }
    return out;
  }

  double layer_path_length(const layer& l, double tx, double ty) {
    return l.thickness * std::sqrt(1. + tx * tx + ty * ty);
  }

  double highland_theta0(double p, double mass, double length, double x0, double charge) {
    if (length <= 0. || p <= 0. || !std::isfinite(p)) {
      return 0.;
    }
    double const beta = p / std::sqrt(p * p + mass * mass);
    double const t    = length / x0;
    double const corr = 1. + 0.038 * std::log(t * charge * charge / (beta * beta));
    return std::max(0., s_highland_e0 / (beta * p) * std::abs(charge) * std::sqrt(t) * corr);
  }

  sym_matrix_t scattering_noise(double theta0, double tx, double ty) {
    double const n2 = 1. + tx * tx + ty * ty;
    double const t2 = theta0 * theta0 * n2;
    sym_matrix_t q;
    q(TX, TX) = t2 * (1. + tx * tx);
    q(TY, TY) = t2 * (1. + ty * ty);
    q(TX, TY) = t2 * tx * ty;
    return q;
  }

  double density_effect_delta(double beta_gamma, const density_effect& d) {
    if (!d.enabled() || beta_gamma <= 0.) {
      return 0.;
    }
    double const two_ln10 = 2. * std::log(10.);
    double const x        = std::log10(beta_gamma);
    if (x >= d.x1) {
      return two_ln10 * x - d.c_bar;
    }
    if (x >= d.x0) {
      return two_ln10 * x - d.c_bar + d.a * std::pow(d.x1 - x, d.k);
    }
    return d.delta0 * std::pow(10., 2. * (x - d.x0));
  }

  double bethe_bloch(double p, double mass, const layer& l, double charge) {
    double const e     = std::sqrt(p * p + mass * mass);
    double const beta2 = p * p / (e * e);
    double const bg2   = p * p / (mass * mass);
    double const gamma = e / mass;
    double const r     = s_electron_mass / mass;
    double const tmax  = 2. * s_electron_mass * bg2 / (1. + 2. * gamma * r + r * r);
    double const arg   = 2. * s_electron_mass * bg2 * tmax / (l.mean_excitation * l.mean_excitation);
    double const delta = density_effect_delta(p / mass, l.sternheimer);
    double const dedx_cm =
        s_bethe_k * charge * charge * l.z_over_a * l.density / beta2 * (0.5 * std::log(arg) - beta2 - 0.5 * delta);
    // dedx_cm in MeV/cm
    return dedx_cm / 10.;
  }

  int energy_loss_sign(int direction, double z_from, double z_to) {
    int const dz = z_to >= z_from ? +1 : -1;
    return dz == direction ? +1 : -1;
  }

  std::optional<material_effects> material_step(const vector_t& par, const layer& l, double mass, int loss_sign) {
    material_effects res;
    res.par           = par;
    res.F             = identity();
    double const qop0 = par[QOP];
    double const tx   = par[TX];
    double const ty   = par[TY];
    double const len  = layer_path_length(l, tx, ty);
    double const s    = loss_sign;
    if (qop0 == 0. || len <= 0.) {
      return res;
    }
    double const q  = qop0 > 0 ? 1. : -1.;
    double const p0 = 1. / std::abs(qop0);
    double const e0 = std::sqrt(p0 * p0 + mass * mass);

    auto dedx_at = [&](double e) -> double {
      if (e <= mass) {
        return -1.;
      }
      return bethe_bloch(std::sqrt(e * e - mass * mass), mass, l);
    };

    // Implicit midpoint by fixed-point iteration: the contraction factor L f'/2 is tiny for thin layers.
    double f0 = dedx_at(e0);
    if (!(f0 > 0.) || !std::isfinite(f0)) {
      return std::nullopt;
    }
    double e1 = e0 - s * len * f0;
    for (int it = 0; it < 50; ++it) {
      double const em = 0.5 * (e0 + e1);
      double const f  = dedx_at(em);
      if (!(f > 0.) || !std::isfinite(f) || e1 <= mass) {
        return std::nullopt;
      }
      double const next = e0 - s * len * f;
      bool const done   = std::abs(next - e1) < 1e-14 * e0;
      e1                = next;
      if (done) {
        break;
      }
    }
    if (e1 <= mass) {
      return std::nullopt;
    }
    double const em = 0.5 * (e0 + e1);
    double const f  = dedx_at(em);
    if (!(f > 0.) || !std::isfinite(f)) {
      return std::nullopt;
    }
    double const p1 = std::sqrt(e1 * e1 - mass * mass);

    // df/dE by central differences, step relative to the kinetic energy at the midpoint
    double const h  = 1e-4 * (em - mass);
    double const fp = (dedx_at(em + h) - dedx_at(em - h)) / (2. * h);

    double const den     = 1. + 0.5 * s * len * fp;
    double const de1_de0 = (1. - 0.5 * s * len * fp) / den;
    double const de1_dl  = -s * f / den;
    double const dq1_de1 = -q * e1 / (p1 * p1 * p1);
    double const n       = std::sqrt(1. + tx * tx + ty * ty);

    res.par[QOP]      = q / p1;
    res.F(QOP, QOP)   = (e1 * p0 * p0 * p0) / (e0 * p1 * p1 * p1) * de1_de0;
    res.F(QOP, TX)    = dq1_de1 * de1_dl * l.thickness * tx / n;
    res.F(QOP, TY)    = dq1_de1 * de1_dl * l.thickness * ty / n;
    res.energy_change = e1 - e0;

    double const pm    = std::sqrt(em * em - mass * mass);
    double const theta = highland_theta0(pm, mass, len, l.radiation_length);
    res.noise          = scattering_noise(theta, tx, ty);
    return res;
  }

} // namespace sand::tracker::kalman
