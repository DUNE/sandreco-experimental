#ifndef CAF_STREAMER_HPP
#define CAF_STREAMER_HPP

#include <ufw/streamer.hpp>

#include <caf/caf_wrapper.hpp>

#include <TFile.h>
#include <TTree.h>

namespace sand::caf {

class caf_streamer : public ufw::streamer {

    std::unique_ptr<TFile> file_{nullptr};
    std::unique_ptr<TTree> tree_{nullptr};
    caf_wrapper* data_ptr_{nullptr};
    ufw::context_id context_id_{};
    long last_entry_{};
    static constexpr auto s_id_brname = "context_id";

public:
    caf_streamer();

    ~caf_streamer() override;

    void configure(const ufw::config&, const ufw::type_id&,
                   ufw::op_type) override;

    void attach(ufw::data::data_base&) override;

    void read(ufw::context_id) override;

    void write(ufw::context_id) override;
};

} // namespace sand::caf

UFW_REGISTER_STREAMER(sand::caf::caf_streamer)

#endif // CAF_STREAMER_HPP
