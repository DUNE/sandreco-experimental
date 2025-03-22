#pragma once

#include <streamer.hpp>

class TFile;

namespace sand {

  namespace common {

    class TTreeStreamer : public ufw::streamer {

      void configure(const ufw::config&) override;

      void attach(const ufw::type_id&, ufw::data::data_base&) override;

      iop support(const ufw::type_id&) const override;

      void read(ufw::context_id) override;

      void write(ufw::context_id) override;

    };

  }

}

UFW_REGISTER_STREAMER(sand::common::TTreeStreamer)
