#include <geoinfo/geoinfo.hpp>
#include <geoinfo/grain_info.hpp>

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
    for (auto name: m_init) {
      UFW_INFO("Initializing: {}", name);
      if (name == "grain") {
        gi.grain();
      } else if (name == "ecal") {
        gi.ecal();
      } else if (name == "tracker") {
        gi.tracker();
      }
    }
    for (auto name: m_test) {
      UFW_INFO("Testing: {}", name);
      if (name == "grain") {
        test_grain();
      } else if (name == "ecal") {
        gi.ecal();
      } else if (name == "tracker") {
        gi.tracker();
      }
    }
  }

  void geoinfo::test_grain() {
    auto close = [](double x, double y) { return std::abs(x - y) < 1e-3; };
    auto area = [](auto r){ return std::abs(r.top - r.bottom) * std::abs(r.right - r.left); };
    sand::geoinfo& gi = instance<sand::geoinfo>();
    //Test that cameras exist and their sipms/holes are the expected size
    double expected_area = 1024 * 2.0 * 2.0;
    for (const auto& lcam: gi.grain().lens_cameras()) {
      UFW_ASSERT(lcam.z_lens > lcam.z_sipm, "Camera {} has the lens behind the sensor", lcam.name);
      double sum = 0.0;
      for (auto r : lcam.sipm_active_areas) {
        sum += area(r);
      }
      UFW_ASSERT(close(sum, expected_area), "Camera {} is not the expected area: {} square mm vs {}", lcam.name, sum, expected_area);
    }
    expected_area = 1024 * 3.0 * 3.0;
    for (const auto& mcam: gi.grain().mask_cameras()) {
      UFW_ASSERT(mcam.z_mask > mcam.z_sipm, "Camera {} has the mask behind the sensor", mcam.name);
      double sum = 0.0;
      for (auto r : mcam.sipm_active_areas) {
        sum += area(r);
      }
      UFW_ASSERT(close(sum, expected_area), "Camera {} is not the expected area: {} square mm vs {}", mcam.name, sum, expected_area);
      sum = 0.0;
      for (auto r : mcam.holes) {
        sum += area(r);
      }
      UFW_ASSERT(close(sum, expected_area / 2.0), "Holes {} are not the expected area: {} square mm vs {}", mcam.name, sum, expected_area / 2.0);
    }
  }

};

UFW_REGISTER_PROCESS(sand::test::geoinfo)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::geoinfo)
