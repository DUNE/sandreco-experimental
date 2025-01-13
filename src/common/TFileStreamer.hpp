#pragma once

#include <streamer.hpp>

class TFile;

namespace sand {

  namespace common {

    class TFileStreamer : public ufw::streamer {

    public:
      ~TFileStreamer() override;

      void configure(const ufw::config&) override;

      iop support(const ufw::type_id&) const override;

      void read(ufw::data&) override;

      void write(const ufw::data&) override;

    private:
      std::unique_ptr<TFile> m_file;
      iop m_mode = iop::none;

    };

  }

}

UFW_REGISTER_STREAMER(sand::common::TFileStreamer)
