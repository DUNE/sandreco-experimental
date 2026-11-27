#include <kalman/filter.hpp>

#include <sstream>
#include <stdexcept>

namespace sand::tracker::kalman {

  sym_matrix_t detail::symmetrize(const matrix_t& C) {
    // Element by element: SMatrix has no assignment from a general to a symmetric matrix (see filter.hpp)
    sym_matrix_t out;
    for (unsigned int i = 0; i < s_dim; ++i) {
      for (unsigned int j = 0; j <= i; ++j) {
        out(i, j) = 0.5 * (C(i, j) + C(j, i));
      }
    }
    return out;
  }

  update_result update(const state& in, const prediction& pred, double measured, double variance, double weight) {
    // Written as !(weight > 0) so that NaN is rejected too. Weight 0 would give V / w = inf and a NaN covariance.
    if (!(weight > 0.)) {
      std::ostringstream msg;
      msg << "sand::tracker::kalman::update: the weight must be positive, got " << weight
          << " (skip the measurement instead of giving it weight 0)";
      throw std::invalid_argument(msg.str());
    }
    update_result res;
    res.out            = in;
    res.residual       = measured - pred.value;
    double const v     = variance / weight;
    vector_t const CHt = in.cov * pred.H;
    double const S     = ROOT::Math::Dot(pred.H, CHt) + v; // variance of the residual
    res.chi2           = res.residual * res.residual / (ROOT::Math::Dot(pred.H, CHt) + variance);

    vector_t const K = CHt / S;
    res.out.par += K * res.residual;

    // Joseph form: (I - K H) C (I - K H)^T + K v K^T; see docs/kalman_filter.md, section 2.8, for its derivation and
    // its equivalence with the short and the information forms
    matrix_t const IKH = matrix_t(ROOT::Math::SMatrixIdentity{}) - ROOT::Math::TensorProd(K, pred.H);
    matrix_t C         = IKH * in.cov * ROOT::Math::Transpose(IKH);
    C += v * ROOT::Math::TensorProd(K, K);
    res.out.cov        = detail::symmetrize(C);
    res.residual_after = res.residual - ROOT::Math::Dot(pred.H, K) * res.residual;
    return res;
  }

  std::optional<std::vector<state>> smooth(const std::vector<filter_step>& steps) {
    std::vector<state> out(steps.size());
    if (steps.empty()) {
      return out;
    }
    out.back() = steps.back().filtered;
    for (int k = static_cast<int>(steps.size()) - 2; k >= 0; --k) {
      auto const& cur   = steps[k];
      auto const& next  = steps[k + 1];
      sym_matrix_t Cinv = next.predicted.cov;
      if (!Cinv.Invert()) {
        return std::nullopt;
      }
      matrix_t const Gk = cur.filtered.cov * ROOT::Math::Transpose(next.F) * Cinv; // smoother gain G_k
      out[k]            = cur.filtered;
      out[k].par += Gk * (out[k + 1].par - next.predicted.par);
      matrix_t const dC = out[k + 1].cov - next.predicted.cov;
      matrix_t const C  = cur.filtered.cov + Gk * dC * ROOT::Math::Transpose(Gk);
      out[k].cov        = detail::symmetrize(C);
    }
    return out;
  }

} // namespace sand::tracker::kalman
