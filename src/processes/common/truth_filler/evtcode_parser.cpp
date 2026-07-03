#include "evtcode_parser.hpp"

#include <ufw/utils.hpp>

#include <charconv>
#include <unordered_map>

namespace sand::common {

  namespace {

    int parse_int(std::string_view sv) {
      int result{};
      std::from_chars(sv.data(), sv.data() + sv.size(), result);
      return result;
    }

    std::optional<std::string_view> after_prefix(std::string_view str, std::string_view prefix) {
      if (str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix) {
        return str.substr(prefix.size());
      }
      return std::nullopt;
    }

    /// Parses `hmult:` payload, e.g. "(p=1,n=2,pi+=1)".
    HadronCounts parse_hmult(std::string_view sv) {
      if (sv.size() >= 2 && sv.front() == '(' && sv.back() == ')') {
        sv = sv.substr(1, sv.size() - 2);
      }

      HadronCounts counts;
      while (!sv.empty()) {
        auto comma = sv.find(',');
        auto entry = sv.substr(0, comma);
        sv         = (comma == std::string_view::npos) ? std::string_view{} : sv.substr(comma + 1);

        const auto eq = entry.find('=');
        if (eq == std::string_view::npos) {
          continue;
        }

        auto key = entry.substr(0, eq);
        int val  = parse_int(entry.substr(eq + 1));

        if (key == "p") {
          counts.n_protons = val;
        } else if (key == "n") {
          counts.n_neutrons = val;
        } else if (key == "pi+") {
          counts.n_pi_plus = val;
        } else if (key == "pi-") {
          counts.n_pi_minus = val;
        } else if (key == "pi0") {
          counts.n_pi_zero = val;
        } else if (key == "gamma") {
          counts.n_gammas = val;
        } else if (key == "rho+") {
          counts.n_rho_plus = val;
        } else if (key == "rho-") {
          counts.n_rho_minus = val;
        } else if (key == "rho0") {
          counts.n_rho_zero = val;
        }
      }
      return counts;
    }

    struct QuarkInfo {
      std::optional<int> pdg{};
      bool is_sea{false};
    };

    /// Parses `q:` payload, e.g. "2(s)" (PDG code, optionally flagged "(s)" for sea quark).
    QuarkInfo parse_quark(std::string_view val) {
      auto paren = val.find('(');
      if (paren == std::string_view::npos) {
        return {};
      }
      return {parse_int(val.substr(0, paren)), paren + 1 < val.size() && val[paren + 1] == 's'};
    }

    struct ProcessInfo {
      std::string interaction_type{};
      ::caf::ScatteringMode scattering_type{};
    };

    /// Parses `proc:` payload, e.g. "Weak[CC],QES" (interaction type, GENIE scattering mode name).
    ProcessInfo parse_process(std::string_view val) {
      static const std::unordered_map<std::string, ::caf::ScatteringMode> mode_map{
          {"QES", ::caf::kQE},
          {"RES", ::caf::kRes},
          {"DIS", ::caf::kDIS},
          {"MEC", ::caf::kMEC},
          {"COH", ::caf::kCoh},
          {"DFR", ::caf::kDiffractive},
          {"1Kaon", ::caf::kSingleKaon},
          {"NuEEL", ::caf::kNuElectronElastic},
          {"IMD", ::caf::kInvMuonDecay},
          {"IMDAnh", ::caf::kIMDAnnihilation},
          {"AMNuGamma", ::caf::kAMNuGamma},
          {"CEvNS", ::caf::kCohElastic},
          {"IBD", ::caf::kInverseBetaDecay},
          {"GLR", ::caf::kGlashowResonance},
          {"PhotonCOH", ::caf::kPhotonCoh},
          {"PhotonRES", ::caf::kPhotonRes},
          {"DMEL", ::caf::kDarkMatterElastic},
          {"DMDIS", ::caf::kDarkMatterDIS},
          {"DME", ::caf::kDarkMatterElectron},
          {"Uknown to GENIE", ::caf::kUnknownMode},
          {"Unknown", ::caf::kUnknownMode},
      };

      const auto comma = val.find(',');
      if (comma == std::string_view::npos) {
        UFW_WARN("Malformed proc token: {}", val);
        return {};
      }

      std::string scattering_str{val.substr(comma + 1)};
      auto it = mode_map.find(scattering_str);
      if (it == mode_map.end()) {
        UFW_WARN("Unknown scattering mode: '{}', defaulting to kUnknownMode", scattering_str);
      }

      return {std::string(val.substr(0, comma)), it != mode_map.end() ? it->second : ::caf::kUnknownMode};
    }

  } // namespace

  EventSummary parse_evt_code(std::string_view input) {
    EventSummary result;

    while (!input.empty()) {
      auto pos   = input.find(';');
      auto token = input.substr(0, pos);
      input      = (pos == std::string_view::npos) ? std::string_view{} : input.substr(pos + 1);

      if (token.empty()) {
        continue;
      }

      if (auto val = after_prefix(token, "nu:")) {
        result.probe_pdg = parse_int(*val);
      } else if (auto val = after_prefix(token, "tgt:")) {
        result.target_pdg = parse_int(*val);
      } else if (auto val = after_prefix(token, "N:")) {
        result.hit_nucleon_pdg = parse_int(*val);
      } else if (auto val = after_prefix(token, "q:")) {
        auto [pdg, is_sea]   = parse_quark(*val);
        result.hit_quark_pdg = pdg;
        result.hit_sea_quark = is_sea;
      } else if (auto val = after_prefix(token, "proc:")) {
        auto [type, mode]       = parse_process(*val);
        result.interaction_type = std::move(type);
        result.scattering_type  = mode;
      } else if (auto val = after_prefix(token, "charm:")) {
        result.is_charm_event = true;
        if (*val != "incl")
          result.charmed_hadron_pdg = parse_int(*val);
      } else if (auto val = after_prefix(token, "strange:")) {
        result.is_strange_event = true;
        if (*val != "incl")
          result.strange_hadron_pdg = parse_int(*val);
      } else if (auto val = after_prefix(token, "hmult:")) {
        result.hmult = parse_hmult(*val);
      } else if (auto val = after_prefix(token, "res:")) {
        result.resonance_type = static_cast<GenieResonanceType>(parse_int(*val));
      } else if (auto val = after_prefix(token, "dec:")) {
        result.decay_mode = parse_int(*val);
      } else if (auto val = after_prefix(token, "finalquark:")) {
        result.final_quark_pdg = parse_int(*val);
      } else if (auto val = after_prefix(token, "finallepton:")) {
        result.final_lepton_pdg = parse_int(*val);
      } else if (token == "dm" || token == "dmb") {
        UFW_ERROR("Dark matter probe '{}' not supported", token);
      }
    }

    return result;
  }

} // namespace sand::common
