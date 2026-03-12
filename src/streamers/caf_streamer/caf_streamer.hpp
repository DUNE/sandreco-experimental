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
    std::unique_ptr<TFile> m_file;
    TTree* m_tree                                    = nullptr; // owned by m_file (ROOT ownership)
    caf_wrapper* m_data                              = nullptr; // non-owning, points to external data
    ::caf::StandardRecord* m_caf_ptr                 = nullptr; // upcast pointer passed to ROOT branch
    ufw::context_id m_context_id                     = {};
    long m_last_entry                                = 0;
    bool m_has_context_id                            = false;
    static constexpr const char* s_context_id_branch = "context_id";
    static constexpr const char* s_data_branch       = "rec";

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
