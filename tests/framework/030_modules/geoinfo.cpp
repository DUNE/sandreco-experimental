#include <geoinfo/ecal_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/grain_info.hpp>
#include <geoinfo/tracker_info.hpp>

#include <ufw/config.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  class geoinfo : public ufw::process {
   public:
    geoinfo();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    void test_grain();
    void test_ecal();
    void test_tracker();

   private:
    std::vector<std::string> m_init;
    std::vector<std::string> m_test;
  };

  geoinfo::geoinfo() : process({}, {}) {}

  void geoinfo::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_init = cfg.value("init", std::vector<std::string>());
    m_test = cfg.value("test", std::vector<std::string>());
  }

  void geoinfo::run() {
    sand::geoinfo& gi = instance<sand::geoinfo>();
    for (auto name : m_init) {
      UFW_INFO("Initializing: {}", name);
      if (name == "grain") {
        gi.grain();
      } else if (name == "ecal") {
        gi.ecal();
      } else if (name == "tracker") {
        gi.tracker();
      }
    }
    for (auto name : m_test) {
      UFW_INFO("Testing: {}", name);
      if (name == "grain") {
        test_grain();
      } else if (name == "ecal") {
        test_ecal();
      } else if (name == "tracker") {
        test_tracker();
      }
    }
  }

  void geoinfo::test_grain() {
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
    expected_area = 1024 * 3.0 * 3.0;
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
  }

  void geoinfo::test_tracker() {
    sand::geoinfo& gi                          = instance<sand::geoinfo>();
    const sand::geoinfo::tracker_info& tracker = gi.tracker();
    auto targets                               = tracker.target_masses();
    static const char* target_names[]          = {"None", "Plastic", "Carbon", "Undefined"};
    UFW_INFO("Target mass report");
    for (auto& [tgt, mass] : targets) {
      UFW_INFO(" - {}:\t\t{} kg", target_names[tgt > 3 ? 3 : tgt], mass);
    }
  }

}; // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::geoinfo)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::geoinfo)
