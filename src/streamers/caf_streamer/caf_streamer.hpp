#ifndef CAF_STREAMER_HPP
#define CAF_STREAMER_HPP

#include <ufw/streamer.hpp>

#include <caf/caf_wrapper.hpp>

#include <TFile.h>
#include <TTree.h>

namespace sand::caf {

  /**
   * @brief Streamer for CAF (Common Analysis Format) ROOT files.
   *
   * Manages reading/writing caf_wrapper data to/from TTree branches.
   */
  class caf_streamer : public ufw::streamer {
    std::unique_ptr<TFile> file_;
    TTree* tree_                                      = nullptr; // owned by file_ (ROOT ownership)
    caf_wrapper* data_ptr_                            = nullptr; // non-owning, points to external data
    ufw::context_id context_id_                       = {};
    long last_entry_                                  = 0;
    static constexpr const char* kContextIdBranchName = "context_id";

   public:
    caf_streamer() = default;
    ~caf_streamer() override;

    void configure(const ufw::config& cfg, ufw::op_type op) override;
    void attach(ufw::data::data_base& data, const ufw::public_id& id) override;
    void read(ufw::context_id id) override;
    void write(ufw::context_id id) override;
  };

} // namespace sand::caf

UFW_REGISTER_STREAMER(sand::caf::caf_streamer)

#endif // CAF_STREAMER_HPP
