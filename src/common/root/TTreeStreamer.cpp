
#include <TFile.h>

#include <config.hpp>
#include <factory.hpp>

#include <TTreeStreamer.hpp>

namespace sand {

  namespace common {

    void TTreeStreamer::configure(const ufw::config& cfg) {
      streamer::configure(cfg);
    }

    void TTreeStreamer::attach(const ufw::type_id&, ufw::data::data_base&) {}

    ufw::streamer::iop TTreeStreamer::support(const ufw::type_id&) const {}

    void TTreeStreamer::read(ufw::context_id) {}

    void TTreeStreamer::write(ufw::context_id) {}

  }

}

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::common::TTreeStreamer)
