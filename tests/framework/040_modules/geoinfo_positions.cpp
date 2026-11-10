#include <geoinfo/ecal_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <geoinfo/grain_info.hpp>
#include <geoinfo/tracker_info.hpp>

#include <ufw/config.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  class geoinfo_positions : public ufw::process {
   public:
    geoinfo_positions();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    pos_3d m_pos;
    dir_3d m_hsize;
    int m_nsamples;

  };

  geoinfo_positions::geoinfo_positions() : process({}, {}) {}

  void geoinfo_positions::configure(const ufw::config& cfg) {
    process::configure(cfg);
    auto p = cfg.at("pos").get<std::array<float, 3>>();
    m_pos.SetX(p[0]);
    m_pos.SetY(p[1]);
    m_pos.SetZ(p[2]);
    auto h = cfg.at("hsize").get<std::array<float, 3>>();
    m_hsize.SetX(h[0]);
    m_hsize.SetY(h[1]);
    m_hsize.SetZ(h[2]);
    m_nsamples = cfg.at("nsamples");
  }

  void geoinfo_positions::run() {
    auto rng = ufw::context::current()->engine();
    static std::uniform_real_distribution<> rnd_unif(0.0, 1.0);
    sand::geoinfo& gi = instance<sand::geoinfo>();
    for (int i = 0; i != m_nsamples; ++i) {
      pos_3d p = m_pos + rnd_unif(rng) * m_hsize;
      UFW_INFO("Position {}: material {}", p, gi.get_info(p).get_material()->GetName());
    }
  }

}; // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::geoinfo_positions)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::geoinfo_positions)
