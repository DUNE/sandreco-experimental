#pragma once

#include <grain.h>

#include <process.hpp>

namespace sand {

  namespace grain {

    class digitize : public ufw::process {

    public:
      void configure (const ufw::config& cfg) override;

      ufw::data_list products() const override;

      ufw::data_list requirements() const override;

      void run(const ufw::data_set&, ufw::data_set&) override;

    };

  }

}

UFW_REGISTER_PROCESS(sand::grain::digitize)
