#ifndef SAND_CAF_CAF_STREAMER_HPP
#define SAND_CAF_CAF_STREAMER_HPP

#include <ufw/streamer.hpp>
#include <duneanaobj/StandardRecord/StandardRecord.h>

class TFile;
class TTree;

namespace caf {
  class StandardRecord;
}

namespace sand::caf {

  /**
   * \class sand::caf::caf_streamer
   *
   * \brief Streamer for CAF (Common Analysis Format) ROOT files.
   *
   * Reads/writes a single `caf::StandardRecord` per event to/from the `rec` branch of a TTree, keyed by
   * an optional `context_id` branch: if present, `read()` does a linear search for the matching
   * `ufw::context_id`; otherwise (plain CAF files) the id is used directly as the entry index. All
   * attached variables share the same underlying `caf::StandardRecord`: each is copied to/from its
   * matching sub-branch (`mc`/`common`/`nd`) on every `read()`/`write()`.
   *
   * \subsection Configuration
   * | Parameter Name | Type   | Unit | Required/Default | Description |
   * |-----------------|--------|------|-------------------|----------------------------------------------------------------------------|
   * | `uri`           | path   |      | Required          | ROOT file to open (mode — READ/RECREATE/UPDATE — set by the
   * task's I/O direction). | | `tree`          | string |      | Required          | Name of the TTree holding the
   * `rec` (and optional `context_id`) branches.  |
   *
   * \subsection Dependencies
   * None.
   *
   * \subsection Supported types
   * |  Type                                    | Comment                                 |
   * |--------------------------------------------|--------------------------------------|
   * | `sand::caf::truth_branch_wrapper`          | Mapped to `caf::StandardRecord::mc`     |
   * | `sand::caf::common_reco_branch_wrapper`    | Mapped to `caf::StandardRecord::common` |
   * | `sand::caf::nd_reco_branch_wrapper`        | Mapped to `caf::StandardRecord::nd`     |
   */
  class caf_streamer : public ufw::streamer {
    std::unique_ptr<TFile> m_file;
    TTree* m_tree{nullptr};
    ::caf::StandardRecord* m_caf_ptr{nullptr};

    ufw::context_id m_context_id{};
    long m_last_entry{};
    bool m_has_context_id{false};

    static constexpr const char* s_context_id_branch{"context_id"};
    static constexpr const char* s_data_branch{"rec"};

   public:
    caf_streamer() = default;
    ~caf_streamer() override;

    void configure(const ufw::config& cfg, ufw::op_type op) override;
    void prepare(const ufw::public_id& id, const ufw::type_id& type) override;
    void attach(ufw::data::data_base& data, const ufw::public_id& id) override;
    void read(ufw::context_id id) override;
    void write(ufw::context_id id) override;
  };

} // namespace sand::caf

UFW_REGISTER_STREAMER(sand::caf::caf_streamer)

#endif // CAF_STREAMER_HPP
