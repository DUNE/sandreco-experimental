#ifndef DEBUG_STREAMER_HPP
#define DEBUG_STREAMER_HPP

#include <ufw/streamer.hpp>

#include <debug/debug_data.hpp>

#include <TFile.h>
#include <TTree.h>

namespace sand::debug {

  /**
   * @brief Streamer useful for ROOT files needed for debugging.
   * It will contain mostly low-level information such as hits or geometry information.
   * Manages reading/writing debug_data data to/from TTree branches.
   */
  class debug_streamer : public ufw::streamer {
    std::unique_ptr<TFile> m_file;
    TTree* m_tree                                    = nullptr; // owned by m_file (ROOT ownership)
    debug_data* m_data                               = nullptr; // non-owning pointer to external debug data
    ufw::context_id m_context_id                     = {};
    long m_last_entry                                = 0;
    bool m_has_context_id                            = false;
    static constexpr const char* s_context_id_branch = "context_id";
    static constexpr const char* s_data_branch       = "debug";


   public:
    debug_streamer() = default;
    ~debug_streamer() override;

    void configure(const ufw::config& cfg, ufw::op_type op) override;
    void attach(ufw::data::data_base& data, const ufw::public_id& id) override;
    void read(ufw::context_id id) override;
    void write(ufw::context_id id) override;
  };

} // namespace sand::debug

UFW_REGISTER_STREAMER(sand::debug::debug_streamer)

#endif // DEBUG_STREAMER_HPP
