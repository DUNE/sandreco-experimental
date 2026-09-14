#ifndef SAND_TRUTH_FILLER_EVTCODE_PARSER_HPP
#define SAND_TRUTH_FILLER_EVTCODE_PARSER_HPP

#include <duneanaobj/StandardRecord/SREnums.h>

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace sand::common {

  /// GENIE baryon resonance codes, as encoded in EvtCode_'s `res:` token.
  enum class GenieResonanceType : std::int8_t {
    NoResonance = -1,
    P33_1232    = 0,  ///< Delta(1232) P33
    S11_1535    = 1,  ///< N(1535) S11
    D13_1520    = 2,  ///< N(1520) D13
    S11_1650    = 3,  ///< N(1650) S11
    D13_1700    = 4,  ///< N(1700) D13
    D15_1675    = 5,  ///< N(1675) D15
    S31_1620    = 6,  ///< Delta(1620) S31
    D33_1700    = 7,  ///< Delta(1700) D33
    P11_1440    = 8,  ///< N(1440) P11 (Roper)
    P33_1600    = 9,  ///< Delta(1600) P33
    P13_1720    = 10, ///< N(1720) P13
    F15_1680    = 11, ///< N(1680) F15
    P31_1910    = 12, ///< Delta(1910) P31
    P33_1920    = 13, ///< Delta(1920) P33
    F35_1905    = 14, ///< Delta(1905) F35
    F37_1950    = 15, ///< Delta(1950) F37
    P11_1710    = 16, ///< N(1710) P11
    F17_1970    = 17  ///< N(1970) F17
  };

  inline std::ostream& operator<< (std::ostream& os, GenieResonanceType r) {
    switch (r) {
    case GenieResonanceType::NoResonance:
      return os << "NoResonance";
    case GenieResonanceType::P33_1232:
      return os << "P33_1232";
    case GenieResonanceType::S11_1535:
      return os << "S11_1535";
    case GenieResonanceType::D13_1520:
      return os << "D13_1520";
    case GenieResonanceType::S11_1650:
      return os << "S11_1650";
    case GenieResonanceType::D13_1700:
      return os << "D13_1700";
    case GenieResonanceType::D15_1675:
      return os << "D15_1675";
    case GenieResonanceType::S31_1620:
      return os << "S31_1620";
    case GenieResonanceType::D33_1700:
      return os << "D33_1700";
    case GenieResonanceType::P11_1440:
      return os << "P11_1440";
    case GenieResonanceType::P33_1600:
      return os << "P33_1600";
    case GenieResonanceType::P13_1720:
      return os << "P13_1720";
    case GenieResonanceType::F15_1680:
      return os << "F15_1680";
    case GenieResonanceType::P31_1910:
      return os << "P31_1910";
    case GenieResonanceType::P33_1920:
      return os << "P33_1920";
    case GenieResonanceType::F35_1905:
      return os << "F35_1905";
    case GenieResonanceType::F37_1950:
      return os << "F37_1950";
    case GenieResonanceType::P11_1710:
      return os << "P11_1710";
    case GenieResonanceType::F17_1970:
      return os << "F17_1970";
    }
    return os << "GenieResonanceType(" << static_cast<int>(r) << ")";
  }

  /// Post-FSI hadron multiplicities, from EvtCode_'s `hmult:` token.
  struct HadronCounts {
    int n_protons{};
    int n_neutrons{};
    int n_pi_plus{};
    int n_pi_minus{};
    int n_pi_zero{};
    int n_gammas{};
    int n_rho_plus{};
    int n_rho_minus{};
    int n_rho_zero{};
  };

  /// Parsed representation of GENIE's EvtCode_ string.
  /// Example input: "nu:14;tgt:1000180400;N:2212;proc:Weak[CC],QES"
  struct EventSummary {
    int probe_pdg{};
    int target_pdg{};
    std::optional<int> hit_nucleon_pdg{};
    std::optional<int> hit_quark_pdg{};
    bool hit_sea_quark{false};

    std::string interaction_type{};
    ::caf::ScatteringMode scattering_type{};

    bool is_charm_event{false};
    std::optional<int> charmed_hadron_pdg{};

    bool is_strange_event{false};
    std::optional<int> strange_hadron_pdg{};

    std::optional<GenieResonanceType> resonance_type{};
    std::optional<int> decay_mode{};
    std::optional<int> final_quark_pdg{};
    std::optional<int> final_lepton_pdg{};

    HadronCounts hmult{};
  };

  /// Parses a GENIE `EvtCode_` string (`;`-separated `key:value` tokens) into an EventSummary.
  [[nodiscard]] EventSummary parse_evt_code(std::string_view evt_code);

} // namespace sand::common

#endif // SAND_TRUTH_FILLER_EVTCODE_PARSER_HPP
