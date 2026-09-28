#ifndef SAND_CAF_CAF_RNTUPLE_STREAMER_HPP
#define SAND_CAF_CAF_RNTUPLE_STREAMER_HPP

#include <ufw/streamer.hpp>

#include <duneanaobj/StandardRecord/StandardRecord.h>

#include <ROOT/REntry.hxx>
#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleWriter.hxx>

#include <cstdint>
#include <memory>

namespace sand::caf {

  /**
   * @brief Streams CAF branches to/from a ROOT RNTuple.
   *
   * The whole caf::StandardRecord is stored as a single RNTuple field named `rec`. RNTuple splits every leaf member
   * into its own column, using the dictionary provided by duneanaobj, so the output can be read directly with
   * RDataFrame (e.g. column `rec.nd.sand.ixn.tracker.ntracks`).
   *
   * Supported data: truth_branch_wrapper, common_reco_branch_wrapper, nd_reco_branch_wrapper.
   */
  class caf_rntuple_streamer : public ufw::streamer {
    std::unique_ptr<ROOT::RNTupleWriter> m_writer;
    std::unique_ptr<ROOT::RNTupleReader> m_reader;
    std::unique_ptr<ROOT::REntry> m_read_entry;

    std::shared_ptr<::caf::StandardRecord> m_record;
    std::shared_ptr<std::uint64_t> m_context_id;

    std::uint64_t m_last_entry{};
    bool m_has_context_id{false};

    static constexpr const char* s_context_id_field{"context_id"};
    static constexpr const char* s_data_field{"rec"};

    void load_entry(std::uint64_t entry);

   public:
    caf_rntuple_streamer() = default;
    ~caf_rntuple_streamer() override;

    void configure(const ufw::config& cfg, ufw::op_type op) override;
    void prepare(const ufw::public_id& id, const ufw::type_id& type) override;
    void read(ufw::context_id id) override;
    void write(ufw::context_id id) override;
  };

} // namespace sand::caf

UFW_REGISTER_STREAMER(sand::caf::caf_rntuple_streamer)

#endif // SAND_CAF_CAF_RNTUPLE_STREAMER_HPP
