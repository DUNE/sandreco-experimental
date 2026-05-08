#include <geoinfo/geoinfo.hpp>

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
    std::vector<std::string> m_init;

  };

  geoinfo::geoinfo() : process({}, {}) {}

  void geoinfo::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_init = cfg.value("init", std::vector<std::string>());
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
  }

};

UFW_REGISTER_PROCESS(sand::test::geoinfo)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::geoinfo)
