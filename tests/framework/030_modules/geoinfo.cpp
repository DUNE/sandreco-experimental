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

  };

  geoinfo::geoinfo() : process({}, {}) {}

  void geoinfo::configure(const ufw::config& cfg) {
    process::configure(cfg);
  }

  void geoinfo::run() {
    sand::geoinfo& gi = instance<sand::geoinfo>();
  }

};

UFW_REGISTER_PROCESS(sand::test::geoinfo)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::geoinfo)
