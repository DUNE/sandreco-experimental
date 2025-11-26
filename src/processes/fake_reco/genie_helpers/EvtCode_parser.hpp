//
// Created by root on 11/26/25.
//

#ifndef SANDRECO_EVTCODE_PARSER_HPP
#define SANDRECO_EVTCODE_PARSER_HPP

#include <duneanaobj/StandardRecord/SREnums.h>

#include <charconv>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace sand::fake_reco {
  struct EventSummary {
    const static std::unordered_map<std::string, caf::ScatteringMode> caf_scattering_mode_map;

    int probe_pdg  = 0;
    int target_pdg = 0;

    // Optional fields (using std::optional for C++17 semantics)
    std::optional<int> hit_nucleon_pdg;
    std::optional<int> hit_quark_pdg;
    bool hit_sea_quark = false; // true if (s), false if (v)

    // TODO: reason on the option to switch to string_view for these members
    std::string interaction_type;
    caf::ScatteringMode scattering_type;
    std::string exclusive_tag;

    EventSummary(std::string_view string) {
      // Helper lambda to consume a prefix and return the rest of the view
      auto remove_prefix = [](const std::string_view sv, const size_t n) { return sv.substr(n); };

      // Helper to parse an integer from a view
      auto parse_int = [](const std::string_view sv) -> int {
        int result = 0;
        std::from_chars(sv.data(), sv.data() + sv.size(), result);
        return result;
      };

      // Loop through the string by splitting at ';'
      while (!string.empty()) {
        // Find position of the next delimiter
        size_t pos = string.find(';');

        // Extract the current token (key:value or flag)
        std::string_view token = string.substr(0, pos);

        // Advance input for the next iteration
        string = (pos == std::string_view::npos) ? "" : string.substr(pos + 1);

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

        // 6. Exclusive Tag (Remainder or unidentified token)
        else {
          // In your logic, the xcls tag is appended at the end.
          // If it has no prefix logic, it falls here.
          exclusive_tag = std::string(token);
        }
      }
    }
  };

  const std::unordered_map<std::string, caf::ScatteringMode> EventSummary::caf_scattering_mode_map{
      {"QES", caf::kQE},
      {"1Kaon", caf::kSingleKaon},
      {"DIS", caf::kDIS},
      {"RES", caf::kRes},
      {"COH", caf::kCoh},
      {"DFR", caf::kDiffractive},
      {"NuEEL", caf::kNuElectronElastic},
      {"IMD", caf::kInvMuonDecay},
      {"AMNuGamma", caf::kAMNuGamma},
      {"MEC", caf::kMEC},
      {"CEvNS", caf::kCohElastic},
      {"IBD", caf::kInverseBetaDecay},
      {"GLR", caf::kGlashowResonance},
      {"IMDAnh", caf::kIMDAnnihilation},
      {"PhotonCOH", caf::kPhotonCoh},
      {"PhotonRES", caf::kPhotonRes},
      // TODO: Check that the absence of a "kScSinglePion" caf equivalent is intended.
      {"DMEL", caf::kDarkMatterElastic},
      {"DMDIS", caf::kDarkMatterDIS},
      {"DME", caf::kDarkMatterElectron},
      // TODO: Check that the absence of a "kScNorm" caf equivalent is intended.

      // There is just one "Unknown mode in cafs"
      {"Uknown to GENIE",
       caf::
           kUnknownMode}, // yes, the typo in "Uknown" is "intended" check
                          // https://github.com/GENIE-MC/Generator/blob/2084cc6b8f25a460ebf4afd6a4658143fa9ce2ff/src/Framework/Interaction/ScatteringType.h#L70
      {"Unknown", caf::kUnknownMode}

      // Gemini comment:
      //  Cases from the first switch with no clear mapping in the second:
      //  kScNorm, kScSinglePion (kScSinglePion is generally considered RES)
  };
} // namespace sand::fake_reco

#endif // SANDRECO_EVTCODE_PARSER_HPP
