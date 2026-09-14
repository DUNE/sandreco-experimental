#include <cmath>

#include <common/array.h>

#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  struct streamer_multiread_read : public ufw::process {
    streamer_multiread_read();
    void run() override;
  };

  streamer_multiread_read::streamer_multiread_read()
    : process({{"in_a", "sand::array<double>"}, {"in_b", "sand::array<double>"}}, {}) {}

  void streamer_multiread_read::run() {
    const auto& a = get<sand::array<double>>("in_a");
    const auto& b = get<sand::array<double>>("in_b");

    UFW_ASSERT(a.values.size() == 1, "Expected exactly one value for 'a', got {}", a.values.size());
    UFW_ASSERT(b.values.size() == 1, "Expected exactly one value for 'b', got {}", b.values.size());
    UFW_ASSERT(std::abs(a.values[0] - 14.0) < 1.e-9, "Variable 'a' does not match the fixture: {}", a.values[0]);
    UFW_ASSERT(std::abs(b.values[0] - 15.0) < 1.e-9, "Variable 'b' does not match the fixture: {}", b.values[0]);
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::streamer_multiread_read)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::streamer_multiread_read)
