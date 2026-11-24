#include <ecal/digit.h>

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
#include <utility>
#include <vector>

namespace sand::test {

  /**
   * \class sand::test::ecal_digi_reference_check
   *
   * \brief Compares ECAL digits read back by the ROOT streamer with the in-memory reference.
   *
   * Reads the text file written by \ref sand::test::ecal_digi_reference_write in
   * 204_ecal_digi_streamer_write.json and, for every context, checks that the digits read from the
   * ROOT file have the same channel, data source, time, ADC, TOT and photo-electrons (truth index and
   * arrival time), in the same order. Doubles are compared bit by bit. The mismatches of each field
   * are counted over the context and logged before failing. The reference must hold at least one
   * digit with photo-electrons, so that the test cannot pass on empty input.
   *
   * \subsection Configuration
   * | Parameter Name | Type     | Unit | Required/Default | Description                                    |
   * |----------------|----------|------|------------------|------------------------------------------------|
   * | `reference`    | `string` |      | Required         | Reference text file, relative to ufw-basepath. |
   *
   * \subsection Requirements
   * | Name   | Type                           | Comment                          |
   * |--------|--------------------------------|----------------------------------|
   * | `digi` | `sand::ecal::digits_container` | Digits read by the ROOT streamer |
   */
  struct ecal_digi_reference_check : public ufw::process {
    ecal_digi_reference_check();
    ~ecal_digi_reference_check() override;
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    /// One digit of the reference file.
    struct reference_digit {
      uint64_t channel  = 0;
      int source        = 0;
      uint64_t t_bits   = 0;
      uint64_t adc_bits = 0;
      uint64_t tot_bits = 0;
      /// Truth index and bit pattern of the arrival time of each photo-electron.
      std::vector<std::pair<std::size_t, uint64_t>> pes;
    };

    std::map<uint64_t, std::vector<reference_digit>> m_reference;
    std::size_t m_checked_contexts = 0;
    std::size_t m_checked_digits   = 0;
  };

  namespace {
    uint64_t bits(double v) {
      uint64_t b = 0;
      std::memcpy(&b, &v, sizeof(v));
      return b;
    }
  } // namespace

  /// \brief Declares the single requirement \c digi.
  ecal_digi_reference_check::ecal_digi_reference_check() : process({{"digi", "sand::ecal::digits_container"}}, {}) {}

  /// \brief Reports how many digits and contexts were compared.
  ecal_digi_reference_check::~ecal_digi_reference_check() {
    UFW_INFO("Compared {} digits in {} contexts", m_checked_digits, m_checked_contexts);
  }

  /// \brief Loads the reference file and checks that it is not trivially empty.
  void ecal_digi_reference_check::configure(const ufw::config& cfg) {
    process::configure(cfg);
    const auto path = cfg.path_at("reference");
    std::ifstream in(path);
    UFW_ASSERT(in.is_open(), "Cannot open reference file '{}'", path.string());
    std::string line;
    std::size_t with_pes = 0;
    while (std::getline(in, line)) {
      std::istringstream is(line);
      uint64_t ctx = 0;
      reference_digit ref;
      std::size_t npes = 0;
      is >> ctx >> ref.channel >> ref.source >> ref.t_bits >> ref.adc_bits >> ref.tot_bits >> npes;
      UFW_ASSERT(!is.fail(), "Malformed reference line '{}'", line);
      ref.pes.resize(npes);
      for (auto& [index, arrival] : ref.pes) {
        is >> index >> arrival;
      }
      UFW_ASSERT(!is.fail(), "Malformed reference line '{}'", line);
      with_pes += npes > 0 ? 1 : 0;
      m_reference[ctx].push_back(std::move(ref));
    }
    UFW_ASSERT(with_pes > 0, "Reference file '{}' holds no digit with photo-electrons", path.string());
    UFW_INFO("Loaded reference for {} contexts, {} digits with photo-electrons", m_reference.size(), with_pes);
  }

  /// \brief Compares the digits of the current context with the reference.
  void ecal_digi_reference_check::run() {
    const auto& digi = get<sand::ecal::digits_container>("digi");
    const auto ctx   = static_cast<uint64_t>(ufw::context::current()->id());
    const auto it    = m_reference.find(ctx);
    UFW_ASSERT(it != m_reference.end() || digi.digits.empty(), "Context {} has {} digits but no reference", ctx,
               digi.digits.size());
    static const std::vector<reference_digit> none;
    const auto& reference = it != m_reference.end() ? it->second : none;
    UFW_ASSERT(digi.digits.size() == reference.size(), "Context {}: {} digits read, {} in the reference", ctx,
               digi.digits.size(), reference.size());

    // Count the mismatches of every field over the whole context, so that the log shows which members
    // are lost, then fail once.
    std::size_t bad_channel = 0;
    std::size_t bad_source  = 0;
    std::size_t bad_time    = 0;
    std::size_t bad_adc     = 0;
    std::size_t bad_tot     = 0;
    std::size_t bad_pe_ids  = 0;
    std::size_t bad_pe_time = 0;
    std::size_t n_pes       = 0;
    for (std::size_t i = 0; i < reference.size(); ++i) {
      const auto& digit = digi.digits[i];
      const auto& ref   = reference[i];
      std::vector<std::size_t> ids;
      std::vector<uint64_t> arrivals;
      for (const auto& pe : digit.true_hits()) {
        ids.push_back(pe.get());
        arrivals.push_back(bits(pe.arrival_time));
      }
      std::vector<std::size_t> ref_ids;
      std::vector<uint64_t> ref_arrivals;
      for (const auto& [index, arrival] : ref.pes) {
        ref_ids.push_back(index);
        ref_arrivals.push_back(arrival);
      }
      bad_channel += digit.channel().raw != ref.channel ? 1 : 0;
      bad_source += static_cast<int>(digit.data_source()) != ref.source ? 1 : 0;
      bad_time += bits(digit.t()) != ref.t_bits ? 1 : 0;
      bad_adc += bits(digit.adc()) != ref.adc_bits ? 1 : 0;
      bad_tot += bits(digit.tot()) != ref.tot_bits ? 1 : 0;
      bad_pe_ids += ids != ref_ids ? 1 : 0;
      bad_pe_time += arrivals != ref_arrivals ? 1 : 0;
      n_pes += ref.pes.size();
      if (i == 0) {
        UFW_DEBUG("Context {} digit 0: channel {:#x}/{:#x}, time {}, {} photo-electrons ({} expected)", ctx,
                  digit.channel().raw, ref.channel, digit.t(), ids.size(), ref_ids.size());
      }
    }
    UFW_INFO("Context {}: {} digits, {} photo-electrons, mismatches: channel {}, source {}, time {}, ADC {}, TOT {}, "
             "photo-electron ids {}, photo-electron times {}",
             ctx, reference.size(), n_pes, bad_channel, bad_source, bad_time, bad_adc, bad_tot, bad_pe_ids,
             bad_pe_time);
    UFW_ASSERT(bad_channel + bad_source + bad_time + bad_adc + bad_tot + bad_pe_ids + bad_pe_time == 0,
               "Context {}: digits read back differ from the reference", ctx);
    ++m_checked_contexts;
    m_checked_digits += reference.size();
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::ecal_digi_reference_check)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::ecal_digi_reference_check)
