#include <kalman/propagator.hpp>

#include <algorithm>
#include <cmath>

namespace sand::tracker::kalman {

  namespace detail {

    void derivatives(const vector_t& s, int d, double mass, const field_vector& b, vector_t& f, matrix_t& A) {
      double const tx = s[TX], ty = s[TY], q = s[QOP];
      double const n  = std::sqrt(1. + tx * tx + ty * ty);
      double const ax = tx * ty * b[0] - (1. + tx * tx) * b[1] + ty * b[2];
      double const ay = (1. + ty * ty) * b[0] - tx * ty * b[1] - tx * b[2];
      double const g  = std::sqrt(1. + mass * mass * q * q); // 1/beta
      double const kq = d * s_kappa * q;

      f[X]   = tx;
      f[Y]   = ty;
      f[TX]  = kq * n * ax;
      f[TY]  = kq * n * ay;
      f[QOP] = 0.;
      f[T]   = d * n * g / s_c;

      double const dax_dtx = ty * b[0] - 2. * tx * b[1];
      double const dax_dty = tx * b[0] + b[2];
      double const day_dtx = -ty * b[1] - b[2];
      double const day_dty = 2. * ty * b[0] - tx * b[1];

      A          = matrix_t();
      A(X, TX)   = 1.;
      A(Y, TY)   = 1.;
      A(TX, TX)  = kq * (tx / n * ax + n * dax_dtx);
      A(TX, TY)  = kq * (ty / n * ax + n * dax_dty);
      A(TX, QOP) = d * s_kappa * n * ax;
      A(TY, TX)  = kq * (tx / n * ay + n * day_dtx);
      A(TY, TY)  = kq * (ty / n * ay + n * day_dty);
      A(TY, QOP) = d * s_kappa * n * ay;
      A(T, TX)   = d * tx / n * g / s_c;
      A(T, TY)   = d * ty / n * g / s_c;
      A(T, QOP)  = d * n * (mass * mass * q / g) / s_c;
    }

  } // namespace detail

  std::optional<propagation_result> propagate(const state& in, double z_target, double mass, const field& bfield,
                                              const propagator_config& cfg) {
    propagation_result res;
    res.F          = identity();
    vector_t s     = in.par;
    double z       = in.z;
    double const L = z_target - z;
    int const nst  = std::max(1, static_cast<int>(std::ceil(std::abs(L) / cfg.max_step)));
    double const h = L / nst;
    int const d    = in.direction;

    vector_t k1, k2, k3, k4;
    matrix_t A1, A2, A3, A4;
    for (int i = 0; i < nst; ++i) {
      auto const b1 = bfield.at(s[X], s[Y], z);
      detail::derivatives(s, d, mass, b1, k1, A1);
      matrix_t const K1 = A1 * res.F;

      vector_t const s2 = s + 0.5 * h * k1;
      auto const b2     = bfield.at(s2[X], s2[Y], z + 0.5 * h);
      detail::derivatives(s2, d, mass, b2, k2, A2);
      matrix_t const K2 = A2 * (res.F + 0.5 * h * K1);

      vector_t const s3 = s + 0.5 * h * k2;
      detail::derivatives(s3, d, mass, b2, k3, A3);
      matrix_t const K3 = A3 * (res.F + 0.5 * h * K2);

      vector_t const s4 = s + h * k3;
      auto const b4     = bfield.at(s4[X], s4[Y], z + h);
      detail::derivatives(s4, d, mass, b4, k4, A4);
      matrix_t const K4 = A4 * (res.F + h * K3);

      double const n0 = std::sqrt(1. + s[TX] * s[TX] + s[TY] * s[TY]);
      s += h / 6. * (k1 + 2. * k2 + 2. * k3 + k4);
      res.F += h / 6. * (K1 + 2. * K2 + 2. * K3 + K4);
      double const n1 = std::sqrt(1. + s[TX] * s[TX] + s[TY] * s[TY]);
      res.path_length += 0.5 * (n0 + n1) * std::abs(h);
      z += h;

      if (std::abs(s[TX]) > cfg.max_slope || std::abs(s[TY]) > cfg.max_slope) {
        return std::nullopt;
      }
    }

    res.out.z         = z_target;
    res.out.par       = s;
    res.out.direction = d;
    res.out.cov       = ROOT::Math::Similarity(res.F, in.cov);
    return res;
  }

  std::optional<propagation_result> propagate(const state& in, double z_target, double mass, const field& bfield,
                                              const material_budget& material, const propagator_config& cfg) {
    auto const layers = material.crossed_layers(in, z_target);
    if (layers.empty()) {
      return propagate(in, z_target, mass, bfield, cfg);
    }
    int const loss_sign = energy_loss_sign(in.direction, in.z, z_target);

    propagation_result res;
    res.F     = identity();
    state cur = in;
    for (auto const& l : layers) {
      auto const seg = propagate(cur, l.z, mass, bfield, cfg);
      if (!seg) {
        return std::nullopt;
      }
      if (l.extent && !l.extent->contains(seg->out.par[X], seg->out.par[Y])) {
        // The track crosses the plane of the layer outside the material: field only
        cur   = seg->out;
        res.F = seg->F * res.F;
        res.path_length += seg->path_length;
        continue;
      }
      auto const mat = material_step(seg->out.par, l, mass, loss_sign);
      if (!mat) {
        return std::nullopt;
      }
      cur     = seg->out;
      cur.par = mat->par;
      cur.cov = ROOT::Math::Similarity(mat->F, seg->out.cov) + mat->noise;
      res.F   = mat->F * seg->F * res.F;
      res.path_length += seg->path_length;
    }
    auto const last = propagate(cur, z_target, mass, bfield, cfg);
    if (!last) {
      return std::nullopt;
    }
    res.out = last->out;
    res.F   = last->F * res.F;
    res.path_length += last->path_length;
    return res;
  }

} // namespace sand::tracker::kalman
