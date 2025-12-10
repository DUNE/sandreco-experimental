//
// Created by Paolo Forni on 11/26/25.
//

#ifndef SANDRECO_EVTCODE_PARSER_HPP
#define SANDRECO_EVTCODE_PARSER_HPP

#include <duneanaobj/StandardRecord/SREnums.h>

#include <charconv>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace sand::fake_reco {
  // Stolen from genie
  namespace genie {

    typedef enum EResonance {
      kNoResonance = -1,
      kP33_1232    = 0,
      kS11_1535    = 1,
      kD13_1520    = 2,
      kS11_1650    = 3,
      kD13_1700    = 4,
      kD15_1675    = 5,
      kS31_1620    = 6,
      kD33_1700    = 7,
      kP11_1440    = 8,
      kP33_1600    = 9,
      kP13_1720    = 10,
      kF15_1680    = 11,
      kP31_1910    = 12,
      kP33_1920    = 13,
      kF35_1905    = 14,
      kF37_1950    = 15,
      kP11_1710    = 16,
      kF17_1970    = 17
    } Resonance_t;

  } // namespace genie
  // end of the stolen code

  // Almost entirely wrote by Gemini 3
  struct EventSummary {
    // Hadron Multiplicities
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

    const static std::unordered_map<std::string, ::caf::ScatteringMode> caf_scattering_mode_map;

    // --- Standard Interaction Fields ---
    int probe_pdg{0};
    int target_pdg{0};

    std::optional<int> hit_nucleon_pdg{};
    std::optional<int> hit_quark_pdg{};
    bool hit_sea_quark{false}; // true if (s), false if (v)

    // TODO: reason on the option to switch to string_view
    std::string interaction_type{};
    ::caf::ScatteringMode scattering_type{};

    // --- Exclusive Tag Fields ---

    // Charm / Strange
    bool is_charm_event{false};
    std::optional<int> charmed_hadron_pdg{}; // nullopt if "incl"

    bool is_strange_event{false};
    std::optional<int> strange_hadron_pdg{}; // nullopt if "incl"

    // Resonance / Decay
    std::optional<genie::Resonance_t> resonance_type{};
    std::optional<int> decay_mode{};

    // Final State
    std::optional<int> final_quark_pdg{};
    std::optional<int> final_lepton_pdg{};

    HadronCounts hmult;

    explicit EventSummary(std::string_view string) {
      // 1. Helper: Consume prefix
      auto remove_prefix = [](const std::string_view sv, const size_t n) { return sv.substr(n); };

      // 2. Helper: Parse int
      auto parse_int = [](const std::string_view sv) -> int {
        int result = 0;
        std::from_chars(sv.data(), sv.data() + sv.size(), result);
        return result;
      };

      // 3. Helper: Parse Hadron Multiplicity String: (p=1,n=2,...)
      auto parse_hmult_string = [&](std::string_view sv) {
        // Remove surrounding parens if present
        if (sv.size() >= 2 && sv.front() == '(' && sv.back() == ')') {
          sv = sv.substr(1, sv.size() - 2);
        }

        while (!sv.empty()) {
          size_t comma_pos      = sv.find(',');
          std::string_view pair = sv.substr(0, comma_pos);
          sv                    = (comma_pos == std::string_view::npos) ? "" : sv.substr(comma_pos + 1);

          size_t eq_pos = pair.find('=');
          if (eq_pos == std::string_view::npos)
            continue;

          std::string_view key = pair.substr(0, eq_pos);
          int val              = parse_int(pair.substr(eq_pos + 1));

          if (key == "p")
            hmult.n_protons = val;
          else if (key == "n")
            hmult.n_neutrons = val;
          else if (key == "pi+")
            hmult.n_pi_plus = val;
          else if (key == "pi-")
            hmult.n_pi_minus = val;
          else if (key == "pi0")
            hmult.n_pi_zero = val;
          else if (key == "gamma")
            hmult.n_gammas = val;
          else if (key == "rho+")
            hmult.n_rho_plus = val;
          else if (key == "rho-")
            hmult.n_rho_minus = val;
          else if (key == "rho0")
            hmult.n_rho_zero = val;
        }
      };

      // --- MAIN LOOP ---
      while (!string.empty()) {
        // Find position of the next delimiter
        size_t pos = string.find(';');

        // Extract the current token (key:value or flag)
        std::string_view token = string.substr(0, pos);

        // Advance input for the next iteration
        string = pos == std::string_view::npos ? "" : string.substr(pos + 1);

        if (token.empty()) {
          continue;
        }

        // --- PARSING LOGIC ---

        // 1. Probe (Flags or Key)
        if (token == "dm" || token == "dmb") {
          // The CafMaker only refers to the variable encoded in the "nu:" case
          UFW_ERROR("Unexpected initial state probe");
        } else if (token.substr(0, 3) == "nu:") {
          probe_pdg = parse_int(remove_prefix(token, 3));
        }

        // 2. Target
        else if (token.substr(0, 4) == "tgt:") {
          target_pdg = parse_int(remove_prefix(token, 4));
        }

        // 3. Hit Nucleon (Optional)
        else if (token.substr(0, 2) == "N:") {
          hit_nucleon_pdg = parse_int(remove_prefix(token, 2));
        }

        // 4. Hit Quark (Optional with suffix)
        else if (token.substr(0, 2) == "q:") {
          // format: q:1(s) or q:2(v)
          std::string_view val = remove_prefix(token, 2);
          size_t suffix_pos    = val.find('(');

          if (suffix_pos != std::string_view::npos) {
            // Parse number part
            hit_quark_pdg = parse_int(val.substr(0, suffix_pos));

            // Check char inside parens (s/v)
            // val[suffix_pos] is '(', val[suffix_pos+1] is 's' or 'v'
            if (suffix_pos + 1 < val.size() && val[suffix_pos + 1] == 's') {
              hit_sea_quark = true;
            }
          }
        }

        // 5. Process Info (Comma separated string)
        else if (token.substr(0, 5) == "proc:") {
          std::string_view vals = remove_prefix(token, 5);
          size_t comma_pos      = vals.find(',');
          if (comma_pos != std::string_view::npos) {
            interaction_type = std::string(vals.substr(0, comma_pos));
            scattering_type  = caf_scattering_mode_map.at(std::string(vals.substr(comma_pos + 1)));
          }
        }

        // --- EXCLUSIVE TAG LOGIC ---

        // Charm: "charm:4122" or "charm:incl"
        else if (token.substr(0, 6) == "charm:") {
          is_charm_event       = true;
          std::string_view val = remove_prefix(token, 6);
          if (val != "incl")
            charmed_hadron_pdg = parse_int(val);
        }

        // Strange: "strange:3122" or "strange:incl"
        else if (token.substr(0, 8) == "strange:") {
          is_strange_event     = true;
          std::string_view val = remove_prefix(token, 8);
          if (val != "incl")
            strange_hadron_pdg = parse_int(val);
        }

        // Hadron Multiplicity: "hmult:(p=1,n=2...)"
        else if (token.substr(0, 6) == "hmult:") {
          parse_hmult_string(remove_prefix(token, 6));
        }

        // Resonance: "res:123"
        else if (token.substr(0, 4) == "res:") {
          resonance_type = static_cast<genie::Resonance_t>(parse_int(remove_prefix(token, 4)));
        }

        // Decay Mode: "dec:1"
        else if (token.substr(0, 4) == "dec:") {
          decay_mode = parse_int(remove_prefix(token, 4));
        }

        // Final Quark: "finalquark:1"
        else if (token.substr(0, 11) == "finalquark:") {
          final_quark_pdg = parse_int(remove_prefix(token, 11));
        }

        // Final Lepton: "finallepton:11"
        else if (token.substr(0, 12) == "finallepton:") {
          final_lepton_pdg = parse_int(remove_prefix(token, 12));
        }
      }
    }
  };

  const std::unordered_map<std::string, ::caf::ScatteringMode> EventSummary::caf_scattering_mode_map{
      {"QES", ::caf::kQE},
      {"1Kaon", ::caf::kSingleKaon},
      {"DIS", ::caf::kDIS},
      {"RES", ::caf::kRes},
      {"COH", ::caf::kCoh},
      {"DFR", ::caf::kDiffractive},
      {"NuEEL", ::caf::kNuElectronElastic},
      {"IMD", ::caf::kInvMuonDecay},
      {"AMNuGamma", ::caf::kAMNuGamma},
      {"MEC", ::caf::kMEC},
      {"CEvNS", ::caf::kCohElastic},
      {"IBD", ::caf::kInverseBetaDecay},
      {"GLR", ::caf::kGlashowResonance},
      {"IMDAnh", ::caf::kIMDAnnihilation},
      {"PhotonCOH", ::caf::kPhotonCoh},
      {"PhotonRES", ::caf::kPhotonRes},
      // TODO: Check that the absence of a "kScSinglePion" caf equivalent is intended.
      {"DMEL", ::caf::kDarkMatterElastic},
      {"DMDIS", ::caf::kDarkMatterDIS},
      {"DME", ::caf::kDarkMatterElectron},
      // TODO: Check that the absence of a "kScNorm" caf equivalent is intended.

      // There is just one "Unknown mode in cafs"
      {"Uknown to GENIE",
       ::caf::
           kUnknownMode}, // yes, the typo in "Uknown" is "intended" check
                          // https://github.com/GENIE-MC/Generator/blob/2084cc6b8f25a460ebf4afd6a4658143fa9ce2ff/src/Framework/Interaction/ScatteringType.h#L70
      {"Unknown", ::caf::kUnknownMode}

      // Gemini comment:
      //  Cases from the first switch with no clear mapping in the second:
      //  kScNorm, kScSinglePion (kScSinglePion is generally considered RES)
  };
} // namespace sand::fake_reco

#endif // SANDRECO_EVTCODE_PARSER_HPP
