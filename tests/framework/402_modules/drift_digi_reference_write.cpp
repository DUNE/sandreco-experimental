#include <tracker/digi.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>

namespace sand::test {

  /**
   * \class sand::test::drift_digi_reference_write
   *
   * \brief Writes the in-memory tracker signals to a plain text reference file.
   *
   * Used by 402_drift_digi_streamer_write.json together with sand::root::tree_streamer: the same
   * digits are written both to a ROOT file and, by this process, to a text file that does not go
   * through any ROOT dictionary. 403_drift_digi_streamer_read.json reads the digits back with the
   * ROOT streamer and compares them with this reference
   * (\ref sand::test::drift_digi_reference_check).
   *
   * One line per signal: context id, raw channel id, data source, bit pattern of the time, bit pattern
   * of the ADC, number of true hits followed by the hit indices. Doubles are stored as their bit
   * pattern so that the comparison is exact.
   *
   * \subsection Configuration
   * | Parameter Name | Type     | Unit | Required/Default | Description                                  |
   * |----------------|----------|------|------------------|----------------------------------------------|
   * | `reference`    | `string` |      | Required         | Output text file, relative to ufw-basepath.  |
   *
   * \subsection Requirements
   * | Name   | Type                  | Comment                    |
   * |--------|-----------------------|----------------------------|
   * | `digi` | `sand::tracker::digi` | Digits to use as reference |
   */
  struct drift_digi_reference_write : public ufw::process {
    drift_digi_reference_write();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    std::ofstream m_out;
  };

  /// \brief Declares the single requirement \c digi.
  drift_digi_reference_write::drift_digi_reference_write() : process({{"digi", "sand::tracker::digi"}}, {}) {}

  /// \brief Opens the reference file, truncating any previous content.
  void drift_digi_reference_write::configure(const ufw::config& cfg) {
    process::configure(cfg);
    const auto path = cfg.path_at("reference", ufw::config::io::overwrite | ufw::config::io::file);
    m_out.open(path, std::ios::trunc);
    UFW_ASSERT(m_out.is_open(), "Cannot open reference file '{}'", path.string());
  }

  /// \brief Appends one line per signal of the current context to the reference file.
  void drift_digi_reference_write::run() {
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto ctx   = static_cast<uint64_t>(ufw::context::current()->id());
    UFW_INFO("Writing {} reference signals for context {}", digi.signals.size(), ctx);
    for (const auto& signal : digi.signals) {
      const double t   = signal.t();
      const double adc = signal.adc();
      uint64_t t_bits  = 0;
      uint64_t a_bits  = 0;
      std::memcpy(&t_bits, &t, sizeof(t));
      std::memcpy(&a_bits, &adc, sizeof(adc));
      m_out << ctx << ' ' << signal.channel().raw << ' ' << static_cast<int>(signal.data_source()) << ' ' << t_bits
            << ' ' << a_bits << ' ' << signal.true_hits().size();
      for (const auto& hit : signal.true_hits()) {
        m_out << ' ' << hit.get();
      }
      m_out << '\n';
    }
    m_out.flush();
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::drift_digi_reference_write)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::drift_digi_reference_write)
