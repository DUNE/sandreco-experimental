#include <kalman/daf.hpp>
#include <kalman/engine.hpp>
#include <kalman/filter.hpp>
#include <kalman/material.hpp>
#include <kalman/propagator.hpp>
#include <kalman/station_material.hpp>

#include <geoinfo/geoinfo.hpp>
#include <geoinfo/tracker_info.hpp>
#include <ufw/config.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <random>
#include <vector>

namespace sand::test {

  /**
   * Checks of the material budget built by sand::tracker::kalman::engine from geoinfo::tracker_info
   * ("material" : "tracker_info"), on the geometry of the job:
   *  - a straight track along z through the centre of the tracker crosses every layer, with a total x/X0 equal to the
   *    sum of station::radiation_lengths();
   *  - the same layers, in reverse order, when propagating towards smaller z (direction -1, and direction +1 as in the
   *    smoother);
   *  - splitting the propagation at the layer centres gives the same layers, none counted twice;
   *  - every layer carries the extent of its station, and a track outside the stations crosses no material;
   *  - engine.propagate() without a budget is identical to propagate() with an equivalent fixed_material_budget;
   *  - engine.fit() without a budget is identical to engine.fit() with the budget of the engine, and differs from the
   *    fit with an empty budget;
   *  - optionally, a toy fit of muons through the tracker material: pulls of the smoothed parameters.
   *
   * Configuration:
   *  - "toy_tracks" (default 0): number of toy muons to fit; 0 disables the toy;
   *  - "pull_mean" (default 0.2), "pull_width" (default [0.85, 1.15]): accepted pull mean (absolute value) and width.
   */
  class kalman_station_material : public ufw::process {
   public:
    kalman_station_material() : process({}, {}) {}
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    void toy_fit(const tracker::kalman::engine& kf, const tracker::kalman::station_material_budget& budget,
                 double x_centre, double y_centre);
    void engine_fit(const tracker::kalman::engine& kf, double x_centre, double y_centre);

    int m_toy_tracks{0};
    double m_pull_mean{0.2};
    std::array<double, 2> m_pull_width{0.85, 1.15};
  };

  namespace {
    using namespace tracker::kalman;
    double constexpr s_mu = 105.6583755; // MeV

    double x_over_x0(const std::vector<layer>& layers) {
      double sum = 0.;
      for (auto const& l : layers) {
        sum += l.thickness / l.radiation_length;
      }
      return sum;
    }

    bool same_layer(const layer& a, const layer& b) {
      return a.z == b.z && a.thickness == b.thickness && a.radiation_length == b.radiation_length
          && a.density == b.density && a.mean_excitation == b.mean_excitation;
    }

    state straight(double z, double x, double y, double tx, double ty, double qop, int direction) {
      state s;
      s.z         = z;
      s.par[X]    = x;
      s.par[Y]    = y;
      s.par[TX]   = tx;
      s.par[TY]   = ty;
      s.par[QOP]  = qop;
      s.direction = direction;
      return s;
    }
  } // namespace

  void kalman_station_material::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_toy_tracks = cfg.value("toy_tracks", m_toy_tracks);
    m_pull_mean  = cfg.value("pull_mean", m_pull_mean);
    m_pull_width = cfg.value("pull_width", m_pull_width);
  }

  void kalman_station_material::run() {
    auto const& tracker = instance<sand::geoinfo>().tracker();
    auto const& kf      = instance<engine>();
    UFW_ASSERT(kf.material() != nullptr, "The engine has no material: configure \"material\" : \"tracker_info\"");
    auto const* budget = dynamic_cast<const station_material_budget*>(kf.material());
    UFW_ASSERT(budget != nullptr, "The material of the engine is not a station_material_budget");

    // Expected budget and z range, from geoinfo directly
    double expected = 0.;
    double z_lo     = std::numeric_limits<double>::max();
    double z_hi     = std::numeric_limits<double>::lowest();
    std::size_t n   = 0;
    for (auto const& st : tracker.stations()) {
      expected += st->radiation_lengths();
      n += st->layers.size();
      if (!st->layers.empty()) {
        z_lo = std::min(z_lo, st->layers.front().z_min);
        z_hi = std::max(z_hi, st->layers.back().z_max);
      }
    }
    UFW_ASSERT(n > 0 && budget->layers().size() == n, "The budget has {} layers, geoinfo {}", budget->layers().size(),
               n);
    z_lo -= 10.;
    z_hi += 10.;

    // A line along z inside every station: the centre of the intersection of the station extents
    double x_min = std::numeric_limits<double>::lowest(), x_max = std::numeric_limits<double>::max();
    double y_min = x_min, y_max = x_max;
    double gx_max = x_min, gy_max = y_min;
    for (auto const& e : budget->extents()) {
      x_min  = std::max(x_min, e.x_min);
      x_max  = std::min(x_max, e.x_max);
      y_min  = std::max(y_min, e.y_min);
      y_max  = std::min(y_max, e.y_max);
      gx_max = std::max(gx_max, e.x_max);
      gy_max = std::max(gy_max, e.y_max);
    }
    UFW_ASSERT(x_min < x_max && y_min < y_max, "The stations have no common transverse area");
    double const xc = 0.5 * (x_min + x_max);
    double const yc = 0.5 * (y_min + y_max);
    UFW_INFO("{} stations, {} layers, z = [{}, {}] mm; common area x = [{}, {}], y = [{}, {}] mm",
             tracker.stations().size(), n, z_lo + 10., z_hi - 10., x_min, x_max, y_min, y_max);

    // Forward along the motion
    auto const forward = budget->crossed_layers(straight(z_lo, xc, yc, 0., 0., -1e-3, +1), z_hi);
    double const total = x_over_x0(forward);
    UFW_INFO("Straight track at the centre: {} layers, x/X0 = {:.6f}; sum of station radiation_lengths() = {:.6f}",
             forward.size(), total, expected);
    UFW_ASSERT(forward.size() == n, "The track at the centre crosses {} layers out of {}", forward.size(), n);
    UFW_ASSERT(std::abs(total - expected) <= 1e-6 * expected, "x/X0 = {} differs from {}", total, expected);
    for (std::size_t i = 1; i < forward.size(); ++i) {
      UFW_ASSERT(forward[i].z > forward[i - 1].z, "Layers not ordered in z at {}", i);
    }

    // Backward: direction -1 (particle moving towards smaller z), and direction +1 propagated backwards (smoother)
    for (int dir : {-1, +1}) {
      auto const backward = budget->crossed_layers(straight(z_hi, xc, yc, 0., 0., -1e-3, dir), z_lo);
      UFW_ASSERT(backward.size() == forward.size(), "Backward (direction {}): {} layers instead of {}", dir,
                 backward.size(), forward.size());
      for (std::size_t i = 0; i < forward.size(); ++i) {
        UFW_ASSERT(same_layer(backward[i], forward[forward.size() - 1 - i]),
                   "Backward (direction {}): layer {} is not the forward one in reverse order", dir, i);
      }
    }
    UFW_INFO("Backward propagations cross the same layers in reverse order.");

    // Split at every layer centre: the segments cross each layer once, for both directions of motion
    for (int dir : {+1, -1}) {
      std::vector<double> planes{z_lo};
      for (auto const& l : forward) {
        planes.push_back(l.z);
      }
      planes.push_back(z_hi);
      if (dir < 0) {
        std::reverse(planes.begin(), planes.end());
      }
      std::vector<layer> joined;
      for (std::size_t i = 1; i < planes.size(); ++i) {
        auto const seg = budget->crossed_layers(straight(planes[i - 1], xc, yc, 0., 0., -1e-3, dir), planes[i]);
        joined.insert(joined.end(), seg.begin(), seg.end());
      }
      UFW_ASSERT(joined.size() == forward.size(), "Split propagation (direction {}): {} layers instead of {}", dir,
                 joined.size(), forward.size());
      for (std::size_t i = 0; i < joined.size(); ++i) {
        auto const& ref = dir > 0 ? forward[i] : forward[forward.size() - 1 - i];
        UFW_ASSERT(same_layer(joined[i], ref), "Split propagation (direction {}): layer {} differs", dir, i);
      }
    }
    UFW_INFO("Propagations split at the layer centres cross each layer once.");

    // Every layer carries the extent of its station
    for (auto const& l : forward) {
      UFW_ASSERT(l.extent.has_value(), "Layer at z = {} mm has no transverse extent", l.z);
    }

    // Outside the stations: crossed_layers returns the layers, the propagation applies none of them. 100 GeV/c, so
    // that the track bends by about 10 mm only and stays outside.
    for (auto const& s :
         {straight(z_lo, gx_max + 100., yc, 0., 0., -1e-5, +1), straight(z_lo, xc, gy_max + 100., 0., 0., -1e-5, +1)}) {
      auto const all = budget->crossed_layers(s, z_hi);
      UFW_ASSERT(all.size() == n, "crossed_layers returned {} layers of {} for a track outside", all.size(), n);
      auto const out = kf.propagate(s, z_hi, s_mu);
      UFW_ASSERT(out, "Propagation outside the stations failed");
      UFW_ASSERT(out->out.par[QOP] == s.par[QOP] && out->out.cov(TX, TX) == 0.,
                 "A track outside the stations at ({}, {}) lost energy or scattered: q/p {} -> {}", s.par[X], s.par[Y],
                 s.par[QOP], out->out.par[QOP]);
    }
    UFW_INFO("Tracks outside the stations cross no material.");

    // Timing of crossed_layers: plane to plane (one call per layer centre) and whole tracker
    {
      std::vector<double> planes{z_lo};
      for (auto const& l : forward) {
        planes.push_back(l.z);
      }
      std::size_t count = 0, calls = 0;
      auto const t0 = std::chrono::steady_clock::now();
      for (int rep = 0; rep < 10; ++rep) {
        for (std::size_t i = 1; i < planes.size(); ++i) {
          count += budget->crossed_layers(straight(planes[i - 1], xc, yc, 0.01, 0.02, -1e-3, +1), planes[i]).size();
          ++calls;
        }
      }
      auto const t1 = std::chrono::steady_clock::now();
      for (int rep = 0; rep < 100; ++rep) {
        count += budget->crossed_layers(straight(z_lo, xc, yc, 0.01, 0.02, -1e-3, +1), z_hi).size();
      }
      auto const t2   = std::chrono::steady_clock::now();
      double const us = std::chrono::duration<double, std::micro>(t1 - t0).count() / calls;
      double const uw = std::chrono::duration<double, std::micro>(t2 - t1).count() / 100.;
      UFW_INFO("crossed_layers: {:.3f} us per call between adjacent layers, {:.1f} us for the whole tracker ({} "
               "layers counted)",
               us, uw, count);
    }

    // Engine material against an equivalent fixed budget, built from geoinfo independently of the engine
    {
      std::vector<layer> all;
      for (auto const& st : tracker.stations()) {
        transverse_extent e{st->top_north.x(), st->top_north.x(), st->top_north.y(), st->top_north.y()};
        for (auto const* c : {&st->top_south, &st->bottom_south, &st->bottom_north}) {
          e.x_min = std::min(e.x_min, c->x());
          e.x_max = std::max(e.x_max, c->x());
          e.y_min = std::min(e.y_min, c->y());
          e.y_max = std::max(e.y_max, c->y());
        }
        for (auto const& m : st->layers) {
          all.push_back(to_layer(m));
          all.back().extent = e;
        }
      }
      fixed_material_budget const fixed(all);
      auto const in = straight(z_lo, xc, yc, 0.002, -0.003, -1. / 3000., +1);
      auto const a  = kf.propagate(in, z_hi, s_mu);
      auto const b  = propagate(in, z_hi, s_mu, kf.field(), fixed, kf.propagator_settings());
      auto const c  = propagate(in, z_hi, s_mu, kf.field(), kf.propagator_settings());
      UFW_ASSERT(a && b && c, "Propagation through the tracker failed");
      for (unsigned int i = 0; i < s_dim; ++i) {
        UFW_ASSERT(a->out.par[i] == b->out.par[i], "Parameter {}: engine {} vs fixed budget {}", i, a->out.par[i],
                   b->out.par[i]);
        for (unsigned int j = 0; j < s_dim; ++j) {
          UFW_ASSERT(a->out.cov(i, j) == b->out.cov(i, j) && a->F(i, j) == b->F(i, j),
                     "Covariance or Jacobian ({}, {}) differs from the fixed budget", i, j);
        }
      }
      double const p_in = in.momentum(), p_out = a->out.momentum();
      UFW_INFO("3 GeV/c muon through the tracker: p = {:.3f} MeV/c after, loss {:.3f} MeV/c; sigma(tx) from "
               "scattering {:.3e}",
               p_out, p_in - p_out, std::sqrt(a->out.cov(TX, TX)));
      UFW_ASSERT(p_out < p_in && c->out.momentum() == p_in, "No energy loss in the engine propagation");
      UFW_INFO("engine.propagate identical to propagate with an equivalent fixed_material_budget.");
    }

    engine_fit(kf, xc, yc);

    if (m_toy_tracks > 0) {
      toy_fit(kf, *budget, xc, yc);
    }
  }

  void kalman_station_material::engine_fit(const engine& kf, double xc, double yc) {
    // Sites on the downstream end of each station's material, one wire per site, alternately along x and along y,
    // 1.5 mm from the track; both sides as candidates. The TDC is the prediction of the true track, propagated with
    // the material of the engine (mean energy loss, no scattering).
    std::vector<double> planes;
    for (auto const& st : instance<sand::geoinfo>().tracker().stations()) {
      if (!st->layers.empty()) {
        planes.push_back(st->layers.back().z_max);
      }
    }
    std::sort(planes.begin(), planes.end());
    UFW_ASSERT(planes.size() >= 6, "engine.fit check: only {} stations with material", planes.size());

    state truth = straight(planes.front(), xc, yc, 0.01, -0.02, -1. / 1500., +1);
    std::vector<site> sites;
    for (std::size_t i = 0; i < planes.size(); ++i) {
      if (i > 0) {
        auto const prop = kf.propagate(truth, planes[i], s_mu);
        UFW_ASSERT(prop.has_value(), "engine.fit check: propagation to plane {} failed", i);
        truth.z   = planes[i];
        truth.par = prop->out.par;
      }
      double const off = (i % 4 < 2 ? 1.5 : -1.5);
      double const x = truth.par[X], y = truth.par[Y];
      wire_geometry w{};
      if (i % 2 == 0) {
        w = {{x - 1000., y + off, planes[i]}, {x + 1000., y + off, planes[i]}};
      } else {
        w = {{x + off, y - 1000., planes[i]}, {x + off, y + 1000., planes[i]}};
      }
      double d = 0., L = 0.;
      detail::wire_distance(truth.par, w, d, L);
      int const side   = d > 0. ? +1 : -1;
      double const tdc = kf.wire_time(truth, w, side).value;
      sites.push_back({planes[i], {{w, tdc, +1, 2 * i}, {w, tdc, -1, 2 * i + 1}}});
    }

    state prior         = straight(planes.front(), xc, yc, 0.01, -0.02, -1. / 1500., +1);
    prior.cov(X, X)     = 1.;
    prior.cov(Y, Y)     = 1.;
    prior.cov(TX, TX)   = 1e-4;
    prior.cov(TY, TY)   = 1e-4;
    prior.cov(QOP, QOP) = std::pow(0.2 / 1500., 2);
    prior.cov(T, T)     = 4.;

    auto const a = kf.fit(prior, sites, s_mu);
    auto const b = kf.fit(prior, sites, s_mu, *kf.material());
    fixed_material_budget const empty({});
    auto const c = kf.fit(prior, sites, s_mu, empty);
    UFW_ASSERT(a.success && b.success && c.success, "engine.fit check: fit failed");
    UFW_ASSERT(a.chi2 == b.chi2 && a.ndf == b.ndf && a.iterations == b.iterations,
               "engine.fit without a budget: chi2 {} ndf {} iterations {}, with the engine budget: {} {} {}", a.chi2,
               a.ndf, a.iterations, b.chi2, b.ndf, b.iterations);
    UFW_ASSERT(a.states.size() == b.states.size(), "engine.fit check: different number of states");
    for (std::size_t k = 0; k < a.states.size(); ++k) {
      for (unsigned int i = 0; i < s_dim; ++i) {
        UFW_ASSERT(a.states[k].par[i] == b.states[k].par[i], "engine.fit check: site {} parameter {}: {} vs {}", k, i,
                   a.states[k].par[i], b.states[k].par[i]);
        for (unsigned int j = 0; j < s_dim; ++j) {
          UFW_ASSERT(a.states[k].cov(i, j) == b.states[k].cov(i, j), "engine.fit check: site {} covariance ({}, {})", k,
                     i, j);
        }
      }
    }
    UFW_ASSERT(a.chi2 != c.chi2 || a.states.front().par[QOP] != c.states.front().par[QOP],
               "engine.fit without a budget gives the same result as with an empty budget");
    UFW_INFO("engine.fit uses the material of the engine: {} sites, chi2 {:.3f} (ndf {:.1f}) with the material, {:.3f} "
             "with an empty budget; q/p on the first site {:.4e} vs {:.4e} (true {:.4e}).",
             sites.size(), a.chi2, a.ndf, c.chi2, a.states.front().par[QOP], c.states.front().par[QOP], -1. / 1500.);
  }

  void kalman_station_material::toy_fit(const engine& kf, const station_material_budget& budget, double xc, double yc) {
    // Measurement planes: the downstream end of each station's material; x and y measured with 0.2 mm resolution.
    std::vector<double> planes;
    for (auto const& st : instance<sand::geoinfo>().tracker().stations()) {
      if (!st->layers.empty()) {
        planes.push_back(st->layers.back().z_max);
      }
    }
    std::sort(planes.begin(), planes.end());
    double const sigma = 0.2;
    std::mt19937_64 rng(20261123);
    std::normal_distribution<double> gaus;
    std::uniform_real_distribution<double> flat;

    sym_matrix_t cov0;
    cov0(X, X)   = 1.;
    cov0(Y, Y)   = 1.;
    cov0(TX, TX) = 1e-4;
    cov0(TY, TY) = 1e-4;
    cov0(T, T)   = 1.;

    std::vector<std::array<double, 5>> pulls;
    int failed    = 0;
    auto const t0 = std::chrono::steady_clock::now();
    for (int itrk = 0; itrk < m_toy_tracks; ++itrk) {
      // Truth: field propagation between layers, mean energy loss and a scattering kick drawn from the noise of
      // material_step on each layer
      double const p = 1000. + 2000. * flat(rng);
      state truth =
          straight(planes.front(), xc + 200. * (2. * flat(rng) - 1.), yc + 200. * (2. * flat(rng) - 1.),
                   0.1 * (2. * flat(rng) - 1.), 0.1 * (2. * flat(rng) - 1.), (flat(rng) < 0.5 ? -1. : 1.) / p, +1);
      std::vector<vector_t> true_par{truth.par};
      bool ok = true;
      for (std::size_t i = 1; i < planes.size() && ok; ++i) {
        for (auto const& l : budget.crossed_layers(truth, planes[i])) {
          auto const at_layer = propagate(truth, l.z, s_mu, kf.field(), kf.propagator_settings());
          if (!at_layer) {
            ok = false;
            break;
          }
          if (l.extent && !l.extent->contains(at_layer->out.par[X], at_layer->out.par[Y])) {
            truth.z   = l.z;
            truth.par = at_layer->out.par;
            continue;
          }
          auto const eff = material_step(at_layer->out.par, l, s_mu, +1);
          if (!eff) {
            ok = false;
            break;
          }
          truth.z   = l.z;
          truth.par = eff->par;
          // correlated (tx, ty) kick with covariance noise(TX..TY)
          double const a = std::sqrt(eff->noise(TX, TX));
          if (a > 0.) {
            double const b  = eff->noise(TX, TY) / a;
            double const c  = std::sqrt(std::max(0., eff->noise(TY, TY) - b * b));
            double const g1 = gaus(rng), g2 = gaus(rng);
            truth.par[TX] += a * g1;
            truth.par[TY] += b * g1 + c * g2;
          }
        }
        auto const to_plane = ok ? propagate(truth, planes[i], s_mu, kf.field(), kf.propagator_settings())
                                 : std::optional<propagation_result>{};
        if (!to_plane) {
          ok = false;
          break;
        }
        truth.z   = planes[i];
        truth.par = to_plane->out.par;
        true_par.push_back(truth.par);
      }
      if (!ok) {
        ++failed;
        continue;
      }

      // Fit: seed drawn from the prior around the truth on the first plane, q/p with 20% resolution
      state seed         = straight(planes.front(), 0., 0., 0., 0., 0., +1);
      seed.cov           = cov0;
      seed.cov(QOP, QOP) = std::pow(0.2 * true_par[0][QOP], 2);
      for (unsigned int k : {X, Y, TX, TY, QOP}) {
        seed.par[k] = true_par[0][k] + std::sqrt(seed.cov(k, k)) * gaus(rng);
      }
      std::vector<filter_step> steps;
      auto measure = [&](const state& in, std::size_t ip) {
        state cur = in;
        for (unsigned int k : {X, Y}) {
          prediction pr;
          pr.value = cur.par[k];
          pr.H[k]  = 1.;
          cur      = update(cur, pr, true_par[ip][k] + sigma * gaus(rng), sigma * sigma).out;
        }
        return cur;
      };
      state current = measure(seed, 0);
      steps.push_back({seed, current, identity()});
      for (std::size_t i = 1; i < planes.size() && ok; ++i) {
        auto const prop = kf.propagate(current, planes[i], s_mu);
        if (!prop) {
          ok = false;
          break;
        }
        current = measure(prop->out, i);
        steps.push_back({prop->out, current, prop->F});
      }
      auto const smoothed = ok ? smooth(steps) : std::nullopt;
      if (!smoothed) {
        ++failed;
        continue;
      }
      auto const& s0 = smoothed->front();
      std::array<double, 5> pl{};
      int j = 0;
      for (unsigned int k : {X, Y, TX, TY, QOP}) {
        pl[j++] = (s0.par[k] - true_par[0][k]) / std::sqrt(s0.cov(k, k));
      }
      pulls.push_back(pl);
    }
    auto const t1 = std::chrono::steady_clock::now();

    UFW_INFO("Toy fit: {} muons, 1-3 GeV/c, {} planes, {} failed, {:.1f} ms per track (simulation and fit)",
             m_toy_tracks, planes.size(), failed,
             std::chrono::duration<double, std::milli>(t1 - t0).count() / m_toy_tracks);
    UFW_ASSERT(pulls.size() >= 0.9 * m_toy_tracks, "Toy fit: only {} of {} tracks fitted", pulls.size(), m_toy_tracks);
    char const* names[] = {"x", "y", "tx", "ty", "q/p"};
    for (int j = 0; j < 5; ++j) {
      double m = 0., m2 = 0.;
      for (auto const& pl : pulls) {
        m += pl[j];
        m2 += pl[j] * pl[j];
      }
      m /= pulls.size();
      double const w = std::sqrt(m2 / pulls.size() - m * m);
      UFW_INFO("Smoothed pull of {} on the first plane: mean {:+.3f}, width {:.3f}", names[j], m, w);
      UFW_ASSERT(std::abs(m) < m_pull_mean && w > m_pull_width[0] && w < m_pull_width[1],
                 "Pull of {} out of range: mean {}, width {}", names[j], m, w);
    }
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::kalman_station_material)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::kalman_station_material)
