#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::ecal {

  class fast_digi : public ufw::process {
   public:
    fast_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
  };
} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::fast_digi)
