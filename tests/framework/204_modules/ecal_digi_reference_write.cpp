#include <ecal/digit.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>

namespace sand::test {

  /**
   * \class sand::test::ecal_digi_reference_write
   *
   * \brief Writes the in-memory ECAL digits to a plain text reference file.
   *
   * Used by 204_ecal_digi_streamer_write.json together with sand::root::tree_streamer: the same
   * digits are written both to a ROOT file and, by this process, to a text file that does not go
   * through any ROOT dictionary. 205_ecal_digi_streamer_read.json reads the digits back with the
   * ROOT streamer and compares them with this reference
   * (\ref sand::test::ecal_digi_reference_check).
   *
   * One line per digit: context id, raw channel id, data source, bit patterns of time, ADC and TOT,
   * number of photo-electrons followed by the truth index and the bit pattern of the arrival time of
   * each one. Doubles are stored as their bit pattern so that the comparison is exact.
   *
   * \subsection Configuration
   * | Parameter Name | Type     | Unit | Required/Default | Description                                 |
   * |----------------|----------|------|------------------|---------------------------------------------|
   * | `reference`    | `string` |      | Required         | Output text file, relative to ufw-basepath. |
   *
   * \subsection Requirements
   * | Name   | Type                           | Comment                    |
   * |--------|--------------------------------|----------------------------|
   * | `digi` | `sand::ecal::digits_container` | Digits to use as reference |
   */
  struct ecal_digi_reference_write : public ufw::process {
    ecal_digi_reference_write();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    std::ofstream m_out;
  };

  namespace {
    uint64_t bits(double v) {
      uint64_t b = 0;
      std::memcpy(&b, &v, sizeof(v));
      return b;
    }
  } // namespace

  /// \brief Declares the single requirement \c digi.
  ecal_digi_reference_write::ecal_digi_reference_write() : process({{"digi", "sand::ecal::digits_container"}}, {}) {}

  /// \brief Opens the reference file, truncating any previous content.
  void ecal_digi_reference_write::configure(const ufw::config& cfg) {
    process::configure(cfg);
    const auto path = cfg.path_at("reference", ufw::config::io::overwrite | ufw::config::io::file);
    m_out.open(path, std::ios::trunc);
    UFW_ASSERT(m_out.is_open(), "Cannot open reference file '{}'", path.string());
  }

  /// \brief Appends one line per digit of the current context to the reference file.
  void ecal_digi_reference_write::run() {
    const auto& digi = get<sand::ecal::digits_container>("digi");
    const auto ctx   = static_cast<uint64_t>(ufw::context::current()->id());
    UFW_INFO("Writing {} reference digits for context {}", digi.digits.size(), ctx);
    for (const auto& digit : digi.digits) {
      m_out << ctx << ' ' << digit.channel().raw << ' ' << static_cast<int>(digit.data_source()) << ' '
            << bits(digit.t()) << ' ' << bits(digit.adc()) << ' ' << bits(digit.tot()) << ' '
            << digit.true_hits().size();
      for (const auto& pe : digit.true_hits()) {
        m_out << ' ' << pe.get() << ' ' << bits(pe.arrival_time);
      }
      m_out << '\n';
    }
    m_out.flush();
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::ecal_digi_reference_write)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::ecal_digi_reference_write)
