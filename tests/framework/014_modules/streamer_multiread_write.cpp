#include <common/array.h>

#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  // Produces two fixed values, later written to the same file/tree by a single root::tree_streamer
  // instance (see 014_streamer_multiread_write.json), so 015_streamer_multiread_read.json can read
  // both back from that one streamer.
  struct streamer_multiread_write : public ufw::process {
    streamer_multiread_write();
    void run() override;
  };

  streamer_multiread_write::streamer_multiread_write()
    : process({}, {{"out_a", "sand::array<double>"}, {"out_b", "sand::array<double>"}}) {}

  void streamer_multiread_write::run() {
    set<sand::array<double>>("out_a").values.push_back(14.0);
    set<sand::array<double>>("out_b").values.push_back(15.0);
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::streamer_multiread_write)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::streamer_multiread_write)
