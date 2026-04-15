#include <common/data.h>

#include <ufw/process.hpp>

namespace sand::ecal {

  class spill_slicer_placeholder : public ufw::process {
   public:
    spill_slicer_placeholder();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
  };
} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::spill_slicer_placeholder)
