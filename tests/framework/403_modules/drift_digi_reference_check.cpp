#include <tracker/digi.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace sand::test {

  /**
   * \class sand::test::drift_digi_reference_check
   *
   * \brief Compares tracker signals read back by the ROOT streamer with the in-memory reference.
   *
   * Reads the text file written by \ref sand::test::drift_digi_reference_write in
   * 402_drift_digi_streamer_write.json and, for every context, checks that the signals read from the
   * ROOT file have the same channel, data source, time, ADC and true hit indices, in the same order.
   * Doubles are compared bit by bit. The reference must hold at least one signal with true hits, so
   * that the test cannot pass on empty input.
   *
   * \subsection Configuration
   * | Parameter Name | Type     | Unit | Required/Default | Description                                  |
   * |----------------|----------|------|------------------|----------------------------------------------|
   * | `reference`    | `string` |      | Required         | Reference text file, relative to ufw-basepath. |
   *
   * \subsection Requirements
   * | Name   | Type                  | Comment                       |
   * |--------|-----------------------|-------------------------------|
   * | `digi` | `sand::tracker::digi` | Digits read by the ROOT streamer |
   */
  struct drift_digi_reference_check : public ufw::process {
    drift_digi_reference_check();
    ~drift_digi_reference_check() override;
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    /// One signal of the reference file.
    struct reference_signal {
      uint64_t channel  = 0;
      int source        = 0;
      uint64_t t_bits   = 0;
      uint64_t adc_bits = 0;
      std::vector<std::size_t> hits;
    };

    std::map<uint64_t, std::vector<reference_signal>> m_reference;
    std::size_t m_checked_contexts = 0;
    std::size_t m_checked_signals  = 0;
  };

  /// \brief Declares the single requirement \c digi.
  drift_digi_reference_check::drift_digi_reference_check() : process({{"digi", "sand::tracker::digi"}}, {}) {}

  /// \brief Reports how many signals and contexts were compared.
  drift_digi_reference_check::~drift_digi_reference_check() {
    UFW_INFO("Compared {} signals in {} contexts", m_checked_signals, m_checked_contexts);
  }

  /// \brief Loads the reference file and checks that it is not trivially empty.
  void drift_digi_reference_check::configure(const ufw::config& cfg) {
    process::configure(cfg);
    const auto path = cfg.path_at("reference");
    std::ifstream in(path);
    UFW_ASSERT(in.is_open(), "Cannot open reference file '{}'", path.string());
    std::string line;
    std::size_t with_hits = 0;
    while (std::getline(in, line)) {
      std::istringstream is(line);
      uint64_t ctx = 0;
      reference_signal ref;
      std::size_t nhits = 0;
      is >> ctx >> ref.channel >> ref.source >> ref.t_bits >> ref.adc_bits >> nhits;
      UFW_ASSERT(!is.fail(), "Malformed reference line '{}'", line);
      ref.hits.resize(nhits);
      for (auto& hit : ref.hits) {
        is >> hit;
      }
      UFW_ASSERT(!is.fail(), "Malformed reference line '{}'", line);
      with_hits += nhits > 0 ? 1 : 0;
      m_reference[ctx].push_back(std::move(ref));
    }
    UFW_ASSERT(with_hits > 0, "Reference file '{}' holds no signal with true hits", path.string());
    UFW_INFO("Loaded reference for {} contexts, {} signals with true hits", m_reference.size(), with_hits);
  }

  /// \brief Compares the signals of the current context with the reference.
  void drift_digi_reference_check::run() {
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto ctx   = static_cast<uint64_t>(ufw::context::current()->id());
    const auto it    = m_reference.find(ctx);
    UFW_ASSERT(it != m_reference.end() || digi.signals.empty(), "Context {} has {} signals but no reference", ctx,
               digi.signals.size());
    static const std::vector<reference_signal> none;
    const auto& reference = it != m_reference.end() ? it->second : none;
    UFW_ASSERT(digi.signals.size() == reference.size(), "Context {}: {} signals read, {} in the reference", ctx,
               digi.signals.size(), reference.size());

    // Count the mismatches of every field over the whole context, so that the log shows which members
    // are lost, then fail once.
    std::size_t bad_channel = 0;
    std::size_t bad_source  = 0;
    std::size_t bad_time    = 0;
    std::size_t bad_adc     = 0;
    std::size_t bad_hits    = 0;
    for (std::size_t i = 0; i < reference.size(); ++i) {
      const auto& signal = digi.signals[i];
      const auto& ref    = reference[i];
      const double t     = signal.t();
      const double adc   = signal.adc();
      uint64_t t_bits    = 0;
      uint64_t a_bits    = 0;
      std::memcpy(&t_bits, &t, sizeof(t));
      std::memcpy(&a_bits, &adc, sizeof(adc));
      std::vector<std::size_t> hits;
      for (const auto& hit : signal.true_hits()) {
        hits.push_back(hit.get());
      }
      bad_channel += signal.channel().raw != ref.channel ? 1 : 0;
      bad_source += static_cast<int>(signal.data_source()) != ref.source ? 1 : 0;
      bad_time += t_bits != ref.t_bits ? 1 : 0;
      bad_adc += a_bits != ref.adc_bits ? 1 : 0;
      bad_hits += hits != ref.hits ? 1 : 0;
      if (i == 0) {
        UFW_DEBUG("Context {} signal 0: channel {:#x}/{:#x}, time {}, {} true hits ({} expected)", ctx,
                  signal.channel().raw, ref.channel, t, hits.size(), ref.hits.size());
      }
    }
    UFW_INFO("Context {}: {} signals, mismatches: channel {}, source {}, time {}, ADC {}, true hits {}", ctx,
             reference.size(), bad_channel, bad_source, bad_time, bad_adc, bad_hits);
    UFW_ASSERT(bad_channel + bad_source + bad_time + bad_adc + bad_hits == 0,
               "Context {}: signals read back differ from the reference", ctx);
    ++m_checked_contexts;
    m_checked_signals += reference.size();
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::drift_digi_reference_check)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::drift_digi_reference_check)
