#ifndef SANDRECO_FAKE_RECO_EVTCODE_PARSER_HPP
#define SANDRECO_FAKE_RECO_EVTCODE_PARSER_HPP

/// @file EvtCode_parser.hpp
/// @brief Parser for GENIE's EvtCode_ interaction string format
///
/// GENIE encodes interaction information in a semicolon-delimited string format.
/// Example: "nu:14;tgt:1000180400;N:2212;proc:Weak[CC],QES"
///
/// This parser extracts all relevant fields into a structured format.

#include <duneanaobj/StandardRecord/SREnums.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sand {

  /// @brief GENIE resonance types (from GENIE's BaryonResonance.h)
  /// @see https://github.com/GENIE-MC/Generator/blob/master/src/Framework/ParticleData/BaryonResonance.h
  namespace genie {

    enum Resonance_t {
      kNoResonance = -1,
      kP33_1232    = 0,  ///< Delta(1232) P33
      kS11_1535    = 1,  ///< N(1535) S11
      kD13_1520    = 2,  ///< N(1520) D13
      kS11_1650    = 3,  ///< N(1650) S11
      kD13_1700    = 4,  ///< N(1700) D13
      kD15_1675    = 5,  ///< N(1675) D15
      kS31_1620    = 6,  ///< Delta(1620) S31
      kD33_1700    = 7,  ///< Delta(1700) D33
      kP11_1440    = 8,  ///< N(1440) P11 (Roper)
      kP33_1600    = 9,  ///< Delta(1600) P33
      kP13_1720    = 10, ///< N(1720) P13
      kF15_1680    = 11, ///< N(1680) F15
      kP31_1910    = 12, ///< Delta(1910) P31
      kP33_1920    = 13, ///< Delta(1920) P33
      kF35_1905    = 14, ///< Delta(1905) F35
      kF37_1950    = 15, ///< Delta(1950) F37
      kP11_1710    = 16, ///< N(1710) P11
      kF17_1970    = 17  ///< N(1970) F17
    };

  } // namespace genie

  /// @brief Parsed representation of GENIE's EvtCode_ string
  ///
  /// Parses the semicolon-delimited interaction string format used by GENIE.
  /// All fields are extracted and stored in a structured format for easy access.
  struct EventSummary {
    /// @brief Hadron multiplicities from the hmult tag
    struct HadronCounts {
      int n_protons   = 0;
      int n_neutrons  = 0;
      int n_pi_plus   = 0;
      int n_pi_minus  = 0;
      int n_pi_zero   = 0;
      int n_gammas    = 0;
      int n_rho_plus  = 0;
      int n_rho_minus = 0;
      int n_rho_zero  = 0;
    };

    // clang-format off
    /// @brief Mapping from GENIE scattering type strings to CAF enums
    /// @note Some GENIE modes have no CAF equivalent:
    ///   - kScSinglePion: Generally treated as RES in CAF
    ///   - kScNorm: Normalization events, no physics content
    /// @note The typo "Uknown" (missing 'n') is intentional - it matches GENIE's typo.
    /// @see https://github.com/GENIE-MC/Generator/blob/master/src/Framework/Interaction/ScatteringType.h
    static const std::unordered_map<std::string, ::caf::ScatteringMode> caf_scattering_mode_map;
    // clang-format on

    // Initial State
    int probe_pdg{0};  ///< Neutrino PDG code (from "nu:" token)
    int target_pdg{0}; ///< Nuclear target PDG code (from "tgt:" token)

    std::optional<int> hit_nucleon_pdg{}; ///< Hit nucleon PDG if available (from "N:" token)
    std::optional<int> hit_quark_pdg{};   ///< Hit quark flavor for DIS (from "q:" token)
    bool hit_sea_quark{false};            ///< true if sea quark "(s)", false if valence "(v)"

    // Process Information
    std::string interaction_type{};          ///< "Weak[CC]", "Weak[NC]", etc.
    ::caf::ScatteringMode scattering_type{}; ///< QE, RES, DIS, MEC, etc.

    // Exclusive Tags
    bool is_charm_event{false};              ///< Event produces charm
    std::optional<int> charmed_hadron_pdg{}; ///< Charmed hadron PDG (nullopt if "incl")

    bool is_strange_event{false};            ///< Event produces strangeness
    std::optional<int> strange_hadron_pdg{}; ///< Strange hadron PDG (nullopt if "incl")

    std::optional<genie::Resonance_t> resonance_type{}; ///< Resonance for RES events
    std::optional<int> decay_mode{};                    ///< Decay mode index

    std::optional<int> final_quark_pdg{};  ///< Final state quark (DIS)
    std::optional<int> final_lepton_pdg{}; ///< Final state lepton

    HadronCounts hmult; ///< Hadron multiplicities

    /// @brief Parse an EvtCode_ string
    /// @param evt_code The GENIE EvtCode_ string to parse
    explicit EventSummary(std::string_view evt_code);

   private:
    /// @brief Parse integer from string_view
    [[nodiscard]] static int parse_int(std::string_view sv);

    /// @brief Check if string starts with prefix and return remainder
    [[nodiscard]] static std::pair<bool, std::string_view> starts_with(std::string_view str, std::string_view prefix);

    /// @brief Parse hadron multiplicity string: "(p=1,n=2,...)"
    void parse_hmult(std::string_view sv);

    /// @brief Parse quark token with sea/valence suffix: "q:1(s)" or "q:2(v)"
    void parse_quark(std::string_view val);

    /// @brief Parse process token: "proc:Weak[CC],QES"
    void parse_process(std::string_view val);

    /// @brief Main parsing function
    void parse(std::string_view input);
  };

} // namespace sand

#endif // SANDRECO_FAKE_RECO_EVTCODE_PARSER_HPP
