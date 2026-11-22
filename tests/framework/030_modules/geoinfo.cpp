#include <geoinfo/ecal_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/grain_info.hpp>
#include <geoinfo/tracker_info.hpp>

#include <ufw/config.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>

namespace sand::test {

  class geoinfo : public ufw::process {
   public:
    geoinfo();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    void test_grain(float mm);
    void test_ecal();
    void test_ecal_neighbours(const sand::geoinfo::ecal_info& ecal,
                              const std::vector<sand::geoinfo::ecal_info::cell_id>& cell_ids);
    void test_tracker();
    void test_tracker_materials(const sand::geoinfo::tracker_info& tracker);

   private:
    std::vector<std::string> m_init;
    std::vector<std::string> m_test;
    /// Optional expectations on the tracker material, from the "tracker_materials" configuration object:
    /// carbon_thickness and c3h6_thickness [mm], target thickness on the test geometry;
    /// carbon_budget [min, max], sum of x/X0 over a graphite station;
    /// non_target_budget [min, max], sum of x/X0 over the layers that are not the target in a graphite station.
    std::optional<double> m_carbon_thickness;
    std::optional<double> m_c3h6_thickness;
    std::optional<std::array<double, 2>> m_carbon_budget;
    std::optional<std::array<double, 2>> m_non_target_budget;
  };

  geoinfo::geoinfo() : process({}, {}) {}

  void geoinfo::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_init = cfg.value("init", std::vector<std::string>());
    m_test = cfg.value("test", std::vector<std::string>());
    if (cfg.contains("tracker_materials")) {
      const auto& tm = cfg.at("tracker_materials");
      if (tm.contains("carbon_thickness")) {
        m_carbon_thickness = tm.at("carbon_thickness").get<double>();
      }
      if (tm.contains("c3h6_thickness")) {
        m_c3h6_thickness = tm.at("c3h6_thickness").get<double>();
      }
      if (tm.contains("carbon_budget")) {
        m_carbon_budget = tm.at("carbon_budget").get<std::array<double, 2>>();
      }
      if (tm.contains("non_target_budget")) {
        m_non_target_budget = tm.at("non_target_budget").get<std::array<double, 2>>();
      }
    }
  }

  void geoinfo::run() {
    sand::geoinfo& gi = instance<sand::geoinfo>();
    for (auto name : m_init) {
      UFW_INFO("Initializing: {}", name);
      if (name == "grain_3mm" || name == "grain_4mm") {
        gi.grain();
      } else if (name == "ecal") {
        gi.ecal();
      } else if (name == "tracker") {
        gi.tracker();
      }
    }
    for (auto name : m_test) {
      UFW_INFO("Testing: {}", name);
      if (name == "grain_3mm") {
        test_grain(3.0);
      } else if (name == "grain_4mm") {
        test_grain(4.0);
      } else if (name == "ecal") {
        test_ecal();
      } else if (name == "tracker") {
        test_tracker();
      }
    }
  }

  void geoinfo::test_grain(float mm) {
    auto close        = [](double x, double y) { return std::abs(x - y) < 1e-3; };
    auto area         = [](auto r) { return std::abs(r.top - r.bottom) * std::abs(r.right - r.left); };
    sand::geoinfo& gi = instance<sand::geoinfo>();
    // Test that cameras exist and their sipms/holes are the expected size
    double expected_area = 1024 * 2.0 * 2.0;
    for (const auto& lcam : gi.grain().lens_cameras()) {
      UFW_ASSERT(lcam.z_lens > lcam.z_sipm, "Camera {} has the lens behind the sensor", lcam.name);
      double sum = 0.0;
      for (auto r : lcam.sipm_active_areas) {
        sum += area(r);
      }
      UFW_ASSERT(close(sum, expected_area), "Camera {} is not the expected area: {} square mm vs {}", lcam.name, sum,
                 expected_area);
    }
    expected_area = mm == 4.0 ? 1008 * 4.0 * 4.0 : 1024 * 3.0 * 3.0;
    for (const auto& mcam : gi.grain().mask_cameras()) {
      UFW_ASSERT(mcam.z_mask > mcam.z_sipm, "Camera {} has the mask behind the sensor", mcam.name);
      double sum = 0.0;
      for (auto r : mcam.sipm_active_areas) {
        sum += area(r);
      }
      UFW_ASSERT(close(sum, expected_area), "Camera {} is not the expected area: {} square mm vs {}", mcam.name, sum,
                 expected_area);
      sum = 0.0;
      for (auto r : mcam.holes) {
        sum += area(r);
      }
      UFW_ASSERT(close(sum, expected_area / 2.0), "Holes {} are not the expected area: {} square mm vs {}", mcam.name,
                 sum, expected_area / 2.0);
    }
  }

  void geoinfo::test_ecal() {
    sand::geoinfo& gi = instance<sand::geoinfo>();

    const auto& ecal = gi.ecal();
    std::vector<sand::geoinfo::ecal_info::cell_id> cell_ids;

    using face_location = sand::geoinfo::ecal_info::face_location;
    using face_side     = sand::geoinfo::ecal_info::face_side;
    using module_key    = std::pair<sand::geo_id::region_t, uint8_t>;
    using side_pair     = std::pair<face_side, face_side>;

    std::map<module_key, std::map<side_pair, size_t>> ecal_side_recap;
    // key = (region, module), value = map of observed begin/end side pairs
    // Example consistent:
    //   (BARREL, 13) -> { (south, north) : 60 cells }
    // Example inconsistent:
    //   (BARREL, 13) -> { (south, north) : 52 cells, (north, south) : 8 cells }

    constexpr std::array<sand::geo_id::region_t, 3> regions = {
        sand::geo_id::region_t::BARREL, sand::geo_id::region_t::ENDCAP_A, sand::geo_id::region_t::ENDCAP_B};

    for (auto region : regions) {
      const uint8_t n_modules = region == sand::geo_id::region_t::BARREL ? 24 : 32;

      for (uint8_t module = 0; module < n_modules; module++) {
        uint8_t n_columns = 12;

        if (region != sand::geo_id::region_t::BARREL) {
          const auto local_module = uint8_t(module % 16);

          if (local_module < 2) {
            n_columns = 6;
          } else if (local_module > 11) {
            n_columns = 2;
          } else {
            n_columns = 3;
          }
        }

        for (uint8_t row = 0; row < 5; row++) {
          for (uint8_t column = 0; column < n_columns; column++) {
            sand::geoinfo::ecal_info::cell_id cid;
            cid.region        = region;
            cid.module_number = module;
            cid.row           = row;
            cid.column        = column;

            const auto& cell = ecal.at(cid);
            cell_ids.push_back(cid);

            const auto begin_side = cell.side(face_location::begin);
            const auto end_side   = cell.side(face_location::end);

            ecal_side_recap[std::make_pair(region, module)][std::make_pair(begin_side, end_side)] += 1;
          }
        }
      }
    }

    bool ecal_side_convention_ok = true;

    UFW_INFO("================ ECAL BEGIN/END SIDE RECAP ================");

    for (const auto& module_entry : ecal_side_recap) {
      const auto region        = module_entry.first.first;
      const auto module_number = module_entry.first.second;
      const auto& side_counts  = module_entry.second;

      const char* region_label = region == sand::geo_id::region_t::BARREL
                                   ? "BARREL"
                                   : (region == sand::geo_id::region_t::ENDCAP_A ? "ENDCAP_A" : "ENDCAP_B");

      if (side_counts.size() == 1) {
        const auto current_side_pair = side_counts.begin()->first;
        const auto begin_side        = current_side_pair.first;
        const auto end_side          = current_side_pair.second;

        bool valid_opposite_sides = false;

        if (region == sand::geo_id::region_t::BARREL) {
          valid_opposite_sides = (begin_side == face_side::north && end_side == face_side::south)
                              || (begin_side == face_side::south && end_side == face_side::north);
        } else if (region == sand::geo_id::region_t::ENDCAP_A || region == sand::geo_id::region_t::ENDCAP_B) {
          valid_opposite_sides = (begin_side == face_side::down && end_side == face_side::up)
                              || (begin_side == face_side::up && end_side == face_side::down);
        }

        if (valid_opposite_sides) {
          UFW_INFO("Module {} {} has begin = {} and end = {} in all {} cells", region_label, module_number, begin_side,
                   end_side, side_counts.begin()->second);
        } else {
          ecal_side_convention_ok = false;

          UFW_WARN("Module {} {} has inconsistent begin/end side convention: begin = {}, end = {}", region_label,
                   module_number, begin_side, end_side);
        }
      } else {
        ecal_side_convention_ok = false;
        for (const auto& side_count : side_counts) {
          UFW_WARN("Module {} {} has inconsistent begin/end side convention.", region_label, module_number);
          UFW_WARN("   begin = {}, end = {} in {} cells", side_count.first.first, side_count.first.second,
                   side_count.second);
        }
      }
    }

    UFW_INFO("===========================================================");

    UFW_ASSERT(ecal_side_convention_ok,
               "[ECAL SIDE CHECK] At least one ECAL module has inconsistent begin/end side convention");
    test_ecal_neighbours(ecal, cell_ids);
  }

  void geoinfo::test_ecal_neighbours(const sand::geoinfo::ecal_info& ecal,
                                     const std::vector<sand::geoinfo::ecal_info::cell_id>& cell_ids) {
    using ecal_info = sand::geoinfo::ecal_info;
    struct cell_geometry {
      ecal_info::cell_id id;
      pos_3d position;
      double y_min = 0.;
      double y_max = 0.;
      std::vector<uint32_t> neighbours;
    };
    std::vector<cell_geometry> cells;
    cells.reserve(cell_ids.size());
    for (const auto cid : cell_ids) {
      const auto& cell = ecal.at(cid);
      cell_geometry geometry{cid, cell.offset2position(0.)};
      // For endcap cells, find the longest straight section along Y and record its min/max Y coordinates.
      // Check that the straight section exists and is non-zero length. This is used to determine if two endcap cells overlap in Y.
      if (cid.region != geo_id::region_t::BARREL) {
        // offset2position(0.) provides the XZ coordinates used for the endcap distance check.
        // Require the long straight sections to overlap in Y as well, so upper and lower central
        // cells with overlapping XZ projections are not counted as neighbours. Ignore the bends.
        double longest_y = 0.;
        for (const auto& element : cell.element_collection().elements()) {
          if (element->type() != ecal_info::shape_element_type::straight) {
            continue;
          }
          const double begin_y  = element->begin_face().centroid().y();
          const double end_y    = element->end_face().centroid().y();
          const double length_y = std::abs(end_y - begin_y);
          if (length_y > longest_y) {
            longest_y      = length_y;
            geometry.y_min = std::min(begin_y, end_y);
            geometry.y_max = std::max(begin_y, end_y);
          }
        }
        UFW_ASSERT(longest_y > 0., "[ECAL NEIGHBOUR CELLS] Cell {} has no straight section along Y", cid.raw);
      }
      for (const auto neighbour : ecal.neighbours(cid)) {
        UFW_ASSERT(ecal.contains(neighbour), "[ECAL NEIGHBOUR CELLS] Cell {} has invalid neighbour {}", cid.raw,
                   neighbour.raw);
        geometry.neighbours.push_back(neighbour.raw);
      }
      std::sort(geometry.neighbours.begin(), geometry.neighbours.end());
      UFW_ASSERT(std::adjacent_find(geometry.neighbours.begin(), geometry.neighbours.end())
                     == geometry.neighbours.end(),
                 "[ECAL NEIGHBOUR CELLS] Cell {} has duplicate neighbours", cid.raw);
      cells.push_back(std::move(geometry));
    }

    // geometry used for test SAND_opt3_DRIFT1: maximum diagonal distances of adjacent cells are 72.2026 mm (barrel) and 66.1026 mm (endcaps).
    // Cells that are not neighbours are at least 88 mm apart in the relevant projection planes. Expected neighbour check uses geometry, not module/row/column ids.
    constexpr double barrel_distance = 72.36;
    constexpr double endcap_distance = 67.;
    constexpr double y_tolerance     = 1.e-6;
    size_t pairs_checked             = 0;
    for (size_t i = 0; i < cells.size(); ++i) {
      const auto& a     = cells[i];
      const bool barrel = a.id.region == geo_id::region_t::BARREL;
      for (size_t j = i; j < cells.size(); ++j) {
        const auto& b = cells[j];
        // transverse distance is in plane YZ for barrel cells, and in plane XZ for endcap cells.
        const double transverse       = barrel ? a.position.y() - b.position.y() : a.position.x() - b.position.x();
        const double dz               = a.position.z() - b.position.z();
        const double distance_squared = transverse * transverse + dz * dz;
        const double threshold        = barrel ? barrel_distance : endcap_distance;
        // Upper/lower central cells overlap in XZ but have disjoint straight-section Y intervals.
        const bool y_overlap = barrel || std::min(a.y_max, b.y_max) - std::max(a.y_min, b.y_min) > y_tolerance;
        const bool expected  = a.id.region == b.id.region && y_overlap && distance_squared < threshold * threshold;
        const bool forward   = std::binary_search(a.neighbours.begin(), a.neighbours.end(), b.id.raw);
        const bool reverse   = std::binary_search(b.neighbours.begin(), b.neighbours.end(), a.id.raw);
        // Expected == false is expected for all cells that are not neighbours.
        UFW_ASSERT(forward == expected && reverse == expected,
                   "[ECAL NEIGHBOUR CELLS] (region,module,row,column) ({},{},{},{}) <-> ({},{},{},{}): "
                   "expected={}, forward={}, reverse={}, projected distance={} mm",
                   int(a.id.region), int(a.id.module_number), int(a.id.row), int(a.id.column), int(b.id.region),
                   int(b.id.module_number), int(b.id.row), int(b.id.column), expected, forward, reverse,
                   std::sqrt(distance_squared));
        ++pairs_checked;
      }
    }
    UFW_INFO("[ECAL NEIGHBOUR CELLS] Checked {} cells and {} pairs (including self) against geometry distances",
             cells.size(), pairs_checked);
  }

  void geoinfo::test_tracker() {
    sand::geoinfo& gi                          = instance<sand::geoinfo>();
    const sand::geoinfo::tracker_info& tracker = gi.tracker();
    auto targets                               = tracker.target_masses();
    static const char* target_names[]          = {"None", "Plastic", "Carbon", "Undefined"};
    UFW_INFO("Target mass report");
    for (auto& [tgt, mass] : targets) {
      UFW_INFO(" - {}:\t\t{} kg", target_names[tgt > 3 ? 3 : tgt], mass);
      UFW_ASSERT(tgt == sand::geoinfo::tracker_info::TRKONLY || mass > 0, "Target mass {} kg is not positive", mass);
    }
    test_tracker_materials(tracker);
  }

  void geoinfo::test_tracker_materials(const sand::geoinfo::tracker_info& tracker) {
    using tracker_info   = sand::geoinfo::tracker_info;
    constexpr double tol = 1e-6; // mm
    // rho * X0 [g/cm2], independent of the density in the geometry: Geant4 10.6.3 G4_GRAPHITE (193.2 mm at
    // 2.21 g/cm3) and G4_POLYPROPYLENE (497.5 mm at 0.900 g/cm3).
    const std::map<std::string, double> x0_g_cm2 = {{"Graphite", 42.70}, {"C3H6", 44.77}};
    // Wire materials: the sampling line must not cross a wire head-on (in the STT it passes between wires).
    const std::array<std::string, 2> wire_materials = {"Gold", "Tungsten"};
    bool printed[3]                                 = {false, false, false};
    std::size_t n_layers                            = 0;
    for (const auto& sptr : tracker.stations()) {
      const tracker_info::station& stat = *sptr;
      const auto& layers                = stat.layers;
      UFW_ASSERT(!layers.empty(), "Station at z = {} mm has no material layers", stat.top_north.z());
      n_layers += layers.size();
      for (std::size_t i = 0; i != layers.size(); ++i) {
        const auto& l = layers[i];
        UFW_ASSERT(l.z_max > l.z_min, "Layer {} ({}) has thickness {} mm", i, l.material, l.thickness());
        UFW_ASSERT(i == 0 || l.z_min >= layers[i - 1].z_max - tol, "Layers {} and {} overlap in z", i - 1, i);
        UFW_ASSERT(l.radiation_length > 0 && l.density > 0, "Layer {} ({}) has X0 {} mm and density {} g/cm3", i,
                   l.material, l.radiation_length, l.density);
        UFW_ASSERT(l.z_over_a > 0.3 && l.z_over_a < 1.0, "Layer {} ({}) has <Z/A> = {}", i, l.material, l.z_over_a);
        UFW_ASSERT(l.mean_excitation > 10e-6 && l.mean_excitation < 1e-3, "Layer {} ({}) has I = {} MeV", i, l.material,
                   l.mean_excitation);
        UFW_ASSERT(std::find(wire_materials.begin(), wire_materials.end(), l.material) == wire_materials.end(),
                   "Layer {} at z [{}, {}] mm is made of wire material {}", i, l.z_min, l.z_max, l.material);
      }
      static const char* station_names[] = {"tracking", "C3H6", "graphite"};
      UFW_INFO("[TRACKER MATERIAL] station z [{:.2f}, {:.2f}] mm {:<8} {:3} layers, radiation_lengths() = {:.5f}",
               layers.front().z_min, layers.back().z_max, station_names[stat.target < 3 ? stat.target : 0],
               layers.size(), stat.radiation_lengths());
      if (stat.target == tracker_info::TRKONLY) {
        continue;
      }
      // The target is the thickest layer of the target material.
      const std::string target_name              = stat.target == tracker_info::CARBON ? "Graphite" : "C3H6";
      const tracker_info::material_layer* target = nullptr;
      for (const auto& l : layers) {
        if (l.material == target_name && (!target || l.thickness() > target->thickness())) {
          target = &l;
        }
      }
      UFW_ASSERT(target, "Station at z = {} mm has no {} layer", stat.top_north.z(), target_name);
      UFW_ASSERT(std::abs(target->thickness() - stat.target_box.z()) < tol,
                 "{} layer is {} mm thick, the target box {} mm", target_name, target->thickness(),
                 stat.target_box.z());
      UFW_ASSERT(std::abs(target->density - stat.target_density) < 1e-9, "Target density mismatch");
      const double rho_x0 = target->density * target->radiation_length / 10.0; // g/cm2
      UFW_ASSERT(std::abs(rho_x0 / x0_g_cm2.at(target_name) - 1.0) < 0.01, "{} has rho X0 = {} g/cm2", target_name,
                 rho_x0);
      UFW_ASSERT(target->sternheimer.has_value(), "{} has no Sternheimer parameters", target_name);
      const auto& expected_thickness = stat.target == tracker_info::CARBON ? m_carbon_thickness : m_c3h6_thickness;
      if (expected_thickness) {
        UFW_ASSERT(std::abs(target->thickness() - *expected_thickness) < tol, "{} target is {} mm thick, expected {}",
                   target_name, target->thickness(), *expected_thickness);
      }
      const double budget = stat.radiation_lengths();
      if (stat.target == tracker_info::CARBON) {
        if (m_carbon_budget) {
          UFW_ASSERT(budget > (*m_carbon_budget)[0] && budget < (*m_carbon_budget)[1],
                     "Graphite station budget {} X0 outside [{}, {}]", budget, (*m_carbon_budget)[0],
                     (*m_carbon_budget)[1]);
        }
        if (m_non_target_budget) {
          const double rest = budget - target->x_over_x0();
          UFW_ASSERT(rest > (*m_non_target_budget)[0] && rest < (*m_non_target_budget)[1],
                     "Graphite station budget without target {} X0 outside [{}, {}]", rest, (*m_non_target_budget)[0],
                     (*m_non_target_budget)[1]);
        }
      }
      if (!printed[stat.target]) {
        printed[stat.target] = true;
        UFW_INFO("[TRACKER MATERIAL] {} station, {} layers, {} X0 in total:", target_name, layers.size(), budget);
        for (const auto& l : layers) {
          UFW_INFO("[TRACKER MATERIAL]   z [{:.4f}, {:.4f}] mm {:9.4f} mm {:<16} X0 {:.1f} mm rho {:.4g} g/cm3 "
                   "Z/A {:.4f} I {:.1f} eV x/X0 {:.3e}{}",
                   l.z_min, l.z_max, l.thickness(), l.material, l.radiation_length, l.density, l.z_over_a,
                   l.mean_excitation * 1e6, l.x_over_x0(), l.sternheimer ? " (density effect)" : "");
        }
      }
    }
    UFW_INFO("[TRACKER MATERIAL] {} stations, {} layers checked", tracker.stations().size(), n_layers);
  }

}; // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::geoinfo)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::geoinfo)
