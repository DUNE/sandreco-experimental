#include <kalman/propagator.hpp>

#include <Geant4/CLHEP/Units/PhysicalConstants.h>
#include <Geant4/CLHEP/Units/SystemOfUnits.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sand::tracker::kalman {

  namespace {
    /// p [MeV] = kappa * B [T] * R [mm] for a unit charge: the field is a number in tesla, see state.hpp
    constexpr double kappa = CLHEP::c_light * CLHEP::tesla;
  } // namespace

  namespace detail {

    void derivatives(const vector_t& s, int sigma, double mass, const field_vector& b, vector_t& f, matrix_t& A) {
      double const tx = s[TX], ty = s[TY], q = s[QOP];
      double const nv = std::sqrt(1. + tx * tx + ty * ty); // |v|, v = (tx, ty, 1)
      double const ax = tx * ty * b[0] - (1. + tx * tx) * b[1] + ty * b[2];
      double const ay = (1. + ty * ty) * b[0] - tx * ty * b[1] - tx * b[2];
      double const g  = std::sqrt(1. + mass * mass * q * q); // 1/beta
      double const kq = sigma * kappa * q;

      f[X]   = tx;
      f[Y]   = ty;
      f[TX]  = kq * nv * ax;
      f[TY]  = kq * nv * ay;
      f[QOP] = 0.;
      f[T]   = sigma * nv * g / CLHEP::c_light; // q/p = 0 gives 1/beta = 1: no mass dependence

      double const dax_dtx = ty * b[0] - 2. * tx * b[1];
      double const dax_dty = tx * b[0] + b[2];
      double const day_dtx = -ty * b[1] - b[2];
      double const day_dty = 2. * ty * b[0] - tx * b[1];

      A          = matrix_t();
      A(X, TX)   = 1.;
      A(Y, TY)   = 1.;
      A(TX, TX)  = kq * (tx / nv * ax + nv * dax_dtx);
      A(TX, TY)  = kq * (ty / nv * ax + nv * dax_dty);
      A(TX, QOP) = sigma * kappa * nv * ax;
      A(TY, TX)  = kq * (tx / nv * ay + nv * day_dtx);
      A(TY, TY)  = kq * (ty / nv * ay + nv * day_dty);
      A(TY, QOP) = sigma * kappa * nv * ay;
      A(T, TX)   = sigma * tx / nv * g / CLHEP::c_light;
      A(T, TY)   = sigma * ty / nv * g / CLHEP::c_light;
      A(T, QOP)  = sigma * nv * (mass * mass * q / g) / CLHEP::c_light;
    }

  } // namespace detail

  std::optional<propagation_result> propagate(const state& in, double z_target, double mass, const field& bfield,
                                              const propagator_config& cfg) {
    // The settings have no defaults: a value-initialised config (zeros) would divide by zero below
    if (!(cfg.max_step > 0.) || !(cfg.max_slope > 0.)) {
      throw std::invalid_argument("sand::tracker::kalman::propagate: max_step and max_slope must be positive (set them "
                                  "from the configuration)");
    }
    propagation_result res;
    res.F           = identity();
    vector_t s      = in.par;
    double z        = in.z;
    double const L  = z_target - z;
    int const nst   = std::max(1, static_cast<int>(std::ceil(std::abs(L) / cfg.max_step)));
    double const h  = L / nst;
    int const sigma = in.direction; // sign of p_z

    // Stage i (notation of docs/kalman_filter.md): the field b_i is read at the position of the stage state, f_i
    // (phi_i in the document) and A_i are evaluated there, and J_i is the matching stage of the Jacobian. Reading the
    // field at every stage makes the scheme a true RK4 in a non-uniform field too; the field gradients are still
    // neglected in A_i.
    vector_t f1, f2, f3, f4;
    matrix_t A1, A2, A3, A4;
    for (int i = 0; i < nst; ++i) {
      auto const b1 = bfield.at(s[X], s[Y], z);
      detail::derivatives(s, sigma, mass, b1, f1, A1);
      matrix_t const J1 = A1 * res.F;

      vector_t const s2 = s + 0.5 * h * f1;
      auto const b2     = bfield.at(s2[X], s2[Y], z + 0.5 * h);
      detail::derivatives(s2, sigma, mass, b2, f2, A2);
      matrix_t const J2 = A2 * (res.F + 0.5 * h * J1);

      vector_t const s3 = s + 0.5 * h * f2;
      auto const b3     = bfield.at(s3[X], s3[Y], z + 0.5 * h);
      detail::derivatives(s3, sigma, mass, b3, f3, A3);
      matrix_t const J3 = A3 * (res.F + 0.5 * h * J2);

      vector_t const s4 = s + h * f3;
      auto const b4     = bfield.at(s4[X], s4[Y], z + h);
      detail::derivatives(s4, sigma, mass, b4, f4, A4);
      matrix_t const J4 = A4 * (res.F + h * J3);

      double const nv0 = std::sqrt(1. + s[TX] * s[TX] + s[TY] * s[TY]);
      s += h / 6. * (f1 + 2. * f2 + 2. * f3 + f4);
      res.F += h / 6. * (J1 + 2. * J2 + 2. * J3 + J4);
      double const nv1 = std::sqrt(1. + s[TX] * s[TX] + s[TY] * s[TY]);
      res.path_length += 0.5 * (nv0 + nv1) * std::abs(h);
      z += h;

      if (std::abs(s[TX]) > cfg.max_slope || std::abs(s[TY]) > cfg.max_slope) {
        return std::nullopt;
      }
    }

    res.out.z         = z_target;
    res.out.par       = s;
    res.out.direction = sigma;
    res.out.cov       = ROOT::Math::Similarity(res.F, in.cov);
    return res;
  }

} // namespace sand::tracker::kalman
