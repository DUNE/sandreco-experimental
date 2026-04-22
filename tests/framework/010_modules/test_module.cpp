#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  class test_module : public ufw::process {
   public:
    test_module();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
  };

  void test_module::configure(const ufw::config& cfg) {
    auto msg = cfg.at("greeting");
    UFW_DEBUG("test_module configured: {}", msg);
  }

  test_module::test_module() : process({}, {}) {
    UFW_INFO("Creating a test_module process at {}", fmt::ptr(this));
  }

  void test_module::run() {
    UFW_DEBUG("test_module run called with context_id: {}", ufw::context::current()->id());
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_module)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_module)
