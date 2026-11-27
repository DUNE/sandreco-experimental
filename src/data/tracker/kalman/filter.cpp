#include <kalman/filter.hpp>

#include <sstream>
#include <stdexcept>

namespace sand::tracker::kalman {

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
    matrix_t IKH(ROOT::Math::SMatrixIdentity{});
    for (unsigned int i = 0; i < s_dim; ++i) {
      for (unsigned int j = 0; j < s_dim; ++j) {
        IKH(i, j) -= K[i] * pred.H[j];
      }
    }
    matrix_t C = IKH * in.cov * ROOT::Math::Transpose(IKH);
    for (unsigned int i = 0; i < s_dim; ++i) {
      for (unsigned int j = 0; j < s_dim; ++j) {
        C(i, j) += K[i] * v * K[j];
      }
    }
    for (unsigned int i = 0; i < s_dim; ++i) {
      for (unsigned int j = 0; j <= i; ++j) {
        res.out.cov(i, j) = 0.5 * (C(i, j) + C(j, i));
      }
    }
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
      matrix_t const Gk = cur.filtered.cov * ROOT::Math::Transpose(next.F) * Cinv;
      out[k]            = cur.filtered;
      out[k].par += Gk * (out[k + 1].par - next.predicted.par);
      matrix_t dC;
      for (unsigned int i = 0; i < s_dim; ++i) {
        for (unsigned int j = 0; j < s_dim; ++j) {
          dC(i, j) = out[k + 1].cov(i, j) - next.predicted.cov(i, j);
        }
      }
      matrix_t const C = cur.filtered.cov + Gk * dC * ROOT::Math::Transpose(Gk); // smoother gain G_k
      for (unsigned int i = 0; i < s_dim; ++i) {
        for (unsigned int j = 0; j <= i; ++j) {
          out[k].cov(i, j) = 0.5 * (C(i, j) + C(j, i));
        }
      }
    }
    return out;
  }

} // namespace sand::tracker::kalman
