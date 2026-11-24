#include <kalman/station_material.hpp>

#include <ufw/utils.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace sand::tracker::kalman {

  density_effect rescale_density_effect(const density_effect& d, double ratio) {
    if (!(ratio > 0.) || !std::isfinite(ratio)) {
      UFW_ERROR("rescale_density_effect: the density ratio must be positive and finite, got {}.", ratio);
    }
    if (!d.enabled()) {
      return d;
    }
    density_effect out = d;
    double const shift = 0.5 * std::log10(ratio);
    out.c_bar          = d.c_bar - std::log(ratio);
    out.x0             = d.x0 - shift;
    out.x1             = d.x1 - shift;
    return out;
  }

  density_effect to_density_effect(const geoinfo::tracker_info::sternheimer_parameters& p, double density) {
    if (!(p.reference_density > 0.)) {
      UFW_ERROR("to_density_effect: the reference density of the Sternheimer parameters must be positive, got {}.",
                p.reference_density);
    }
    density_effect const d{p.a, p.k, p.x0, p.x1, p.C, p.delta0};
    return rescale_density_effect(d, density / p.reference_density);
  }

  layer to_layer(const geoinfo::tracker_info::material_layer& m) {
    layer l;
    l.z                = 0.5 * (m.z_min + m.z_max);
    l.thickness        = m.thickness();
    l.radiation_length = m.radiation_length;
    l.density          = m.density;
    l.z_over_a         = m.z_over_a;
    l.mean_excitation  = m.mean_excitation;
    if (m.sternheimer) {
      l.sternheimer = to_density_effect(*m.sternheimer, m.density);
    }
    return l;
  }

  station_material_budget::station_material_budget(const geoinfo::tracker_info& tracker) {
    auto const& stations = tracker.stations();
    m_extents.reserve(stations.size());
    for (std::size_t i = 0; i < stations.size(); ++i) {
      auto const& st = *stations[i];
      transverse_extent e{st.top_north.x(), st.top_north.x(), st.top_north.y(), st.top_north.y()};
      for (auto const* c : {&st.top_south, &st.bottom_south, &st.bottom_north}) {
        e.x_min = std::min(e.x_min, c->x());
        e.x_max = std::max(e.x_max, c->x());
        e.y_min = std::min(e.y_min, c->y());
        e.y_max = std::max(e.y_max, c->y());
      }
      m_extents.push_back(e);
      for (auto const& m : st.layers) {
        if (!(m.thickness() > 0.) || !(m.radiation_length > 0.) || !(m.density > 0.)) {
          UFW_ERROR(
              "station_material_budget: layer of '{}' in station {} at z = [{}, {}] mm has thickness {} mm, X0 {} "
              "mm, density {} g/cm3; all must be positive.",
              m.material, i, m.z_min, m.z_max, m.thickness(), m.radiation_length, m.density);
        }
        layer l  = to_layer(m);
        l.extent = e;
        m_layers.push_back({l, i});
      }
    }
    std::stable_sort(m_layers.begin(), m_layers.end(),
                     [](const station_layer& a, const station_layer& b) { return a.material.z < b.material.z; });
  }

  std::vector<layer> station_material_budget::crossed_layers(const state& s, double z_target) const {
    std::vector<layer> out;
    if (z_target == s.z) {
      return out;
    }
    double const lo = std::min(s.z, z_target);
    double const hi = std::max(s.z, z_target);
    // Candidates with lo <= z <= hi; the half-open rule is applied by layer_in_interval().
    auto const first = std::lower_bound(m_layers.begin(), m_layers.end(), lo,
                                        [](const station_layer& l, double z) { return l.material.z < z; });
    auto const last =
        std::upper_bound(first, m_layers.end(), hi, [](double z, const station_layer& l) { return z < l.material.z; });
    auto const keep = [&](const station_layer& l) {
      return layer_in_interval(l.material.z, s.z, z_target, s.direction);
    };
    if (z_target > s.z) {
      for (auto it = first; it != last; ++it) {
        if (keep(*it)) {
          out.push_back(it->material);
        }
      }
    } else {
      for (auto it = last; it != first;) {
        --it;
        if (keep(*it)) {
          out.push_back(it->material);
        }
      }
    }
    return out;
  }

} // namespace sand::tracker::kalman
