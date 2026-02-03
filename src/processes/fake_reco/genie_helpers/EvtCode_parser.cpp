#include "EvtCode_parser.hpp"

#include <ufw/utils.hpp>

#include <charconv>

namespace sand {

  // clang-format off
  const std::unordered_map<std::string, ::caf::ScatteringMode> EventSummary::caf_scattering_mode_map{
      // Standard scattering modes
      {"QES",        ::caf::kQE},
      {"RES",        ::caf::kRes},
      {"DIS",        ::caf::kDIS},
      {"MEC",        ::caf::kMEC},
      {"COH",        ::caf::kCoh},
      {"DFR",        ::caf::kDiffractive},

      // Rare processes
      {"1Kaon",      ::caf::kSingleKaon},
      {"NuEEL",      ::caf::kNuElectronElastic},
      {"IMD",        ::caf::kInvMuonDecay},
      {"IMDAnh",     ::caf::kIMDAnnihilation},
      {"AMNuGamma",  ::caf::kAMNuGamma},
      {"CEvNS",      ::caf::kCohElastic},
      {"IBD",        ::caf::kInverseBetaDecay},
      {"GLR",        ::caf::kGlashowResonance},

      // Photon-induced
      {"PhotonCOH",  ::caf::kPhotonCoh},
      {"PhotonRES",  ::caf::kPhotonRes},

      // Dark matter
      {"DMEL",       ::caf::kDarkMatterElastic},
      {"DMDIS",      ::caf::kDarkMatterDIS},
      {"DME",        ::caf::kDarkMatterElectron},

      // Unknown/fallback (note: "Uknown" typo matches GENIE source)
      {"Uknown to GENIE", ::caf::kUnknownMode},
      {"Unknown",         ::caf::kUnknownMode}
  };
  // clang-format on

  EventSummary::EventSummary(std::string_view evt_code) { parse(evt_code); }

  int EventSummary::parse_int(std::string_view sv) {
    int result = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), result);
    return result;
  }

  std::pair<bool, std::string_view> EventSummary::starts_with(std::string_view str, std::string_view prefix) {
    if (str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix) {
      return {true, str.substr(prefix.size())};
    }
    return {false, {}};
  }

  void EventSummary::parse_hmult(std::string_view sv) {
    // Remove surrounding parentheses
    if (sv.size() >= 2 && sv.front() == '(' && sv.back() == ')') {
      sv = sv.substr(1, sv.size() - 2);
    }

    while (!sv.empty()) {
      const std::size_t comma_pos = sv.find(',');
      const std::string_view pair = sv.substr(0, comma_pos);
      sv = (comma_pos == std::string_view::npos) ? std::string_view{} : sv.substr(comma_pos + 1);

      const std::size_t eq_pos = pair.find('=');
      if (eq_pos == std::string_view::npos)
        continue;

      const std::string_view key = pair.substr(0, eq_pos);
      const int val              = parse_int(pair.substr(eq_pos + 1));

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
  }

  void EventSummary::parse_quark(std::string_view val) {
    const std::size_t paren_pos = val.find('(');
    if (paren_pos != std::string_view::npos) {
      hit_quark_pdg = parse_int(val.substr(0, paren_pos));
      // Check for "(s)" = sea quark, "(v)" = valence quark
      if (paren_pos + 1 < val.size() && val[paren_pos + 1] == 's') {
        hit_sea_quark = true;
      }
    }
  }

  void EventSummary::parse_process(std::string_view val) {
    const std::size_t comma_pos = val.find(',');
    if (comma_pos == std::string_view::npos) {
      UFW_WARN("Malformed proc token: {}", val);
      return;
    }

    interaction_type = std::string(val.substr(0, comma_pos));
    const std::string scattering_str{val.substr(comma_pos + 1)};

    auto it = caf_scattering_mode_map.find(scattering_str);
    if (it != caf_scattering_mode_map.end()) {
      scattering_type = it->second;
    } else {
      UFW_WARN("Unknown scattering mode: '{}', defaulting to kUnknownMode", scattering_str);
      scattering_type = ::caf::kUnknownMode;
    }
  }

  void EventSummary::parse(std::string_view input) {
    while (!input.empty()) {
      // Find next delimiter
      const std::size_t pos        = input.find(';');
      const std::string_view token = input.substr(0, pos);
      input                        = (pos == std::string_view::npos) ? std::string_view{} : input.substr(pos + 1);

      if (token.empty())
        continue;

      // Try each token type
      if (auto [ok, val] = starts_with(token, "nu:"); ok) {
        probe_pdg = parse_int(val);
      } else if (auto [ok, val] = starts_with(token, "tgt:"); ok) {
        target_pdg = parse_int(val);
      } else if (auto [ok, val] = starts_with(token, "N:"); ok) {
        hit_nucleon_pdg = parse_int(val);
      } else if (auto [ok, val] = starts_with(token, "q:"); ok) {
        parse_quark(val);
      } else if (auto [ok, val] = starts_with(token, "proc:"); ok) {
        parse_process(val);
      } else if (auto [ok, val] = starts_with(token, "charm:"); ok) {
        is_charm_event = true;
        if (val != "incl")
          charmed_hadron_pdg = parse_int(val);
      } else if (auto [ok, val] = starts_with(token, "strange:"); ok) {
        is_strange_event = true;
        if (val != "incl")
          strange_hadron_pdg = parse_int(val);
      } else if (auto [ok, val] = starts_with(token, "hmult:"); ok) {
        parse_hmult(val);
      } else if (auto [ok, val] = starts_with(token, "res:"); ok) {
        resonance_type = static_cast<genie::Resonance_t>(parse_int(val));
      } else if (auto [ok, val] = starts_with(token, "dec:"); ok) {
        decay_mode = parse_int(val);
      } else if (auto [ok, val] = starts_with(token, "finalquark:"); ok) {
        final_quark_pdg = parse_int(val);
      } else if (auto [ok, val] = starts_with(token, "finallepton:"); ok) {
        final_lepton_pdg = parse_int(val);
      } else if (token == "dm" || token == "dmb") {
        // Dark matter probes - not supported in this context
        UFW_ERROR("Dark matter probe '{}' not supported", token);
      }
      // Unknown tokens are silently ignored
    }
  }

} // namespace sand
