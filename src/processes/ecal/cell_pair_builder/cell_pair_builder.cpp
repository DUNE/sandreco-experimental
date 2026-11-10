#include <cell_pair_builder.hpp>

#include <ecal/cell_pair.h>
#include <ecal/digit.h>

#include <geoinfo/ecal_info.hpp>
#include <geoinfo/geoinfo.hpp>

#include <common/timerange.h>
#include <common/timeslicer.h>

#include <ufw/factory.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace sand::ecal {

  namespace {

    using digit         = sand::ecal::digits_container::digit;
    using cell_id       = sand::geoinfo::ecal_info::cell_id;
    using module_t      = sand::geoinfo::ecal_info::module_t;
    using face_location = sand::geoinfo::ecal_info::face_location;

    struct digit_ref {
      const digit* ptr  = nullptr;
      std::size_t index = 0;
    };

    struct grouped_cell_digits {
      cell_id cid{};
      std::vector<digit_ref> begin;
      std::vector<digit_ref> end;
    };

    std::uint16_t saturated_u16(std::size_t value) {
      constexpr auto max_u16 = static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max());

      if (value > max_u16) {
        return std::numeric_limits<std::uint16_t>::max();
      }

      return static_cast<std::uint16_t>(value);
    }

    /// @brief Check whether a begin/end digit pair is physically compatible.
    ///
    /// Model:
    ///
    ///   t_begin = t0 + d_begin / v
    ///   t_end   = t0 + d_end   / v
    ///   d_begin + d_end = L
    ///
    /// Therefore:
    ///
    ///   d_begin = 0.5 * (L + v * (t_begin - t_end))
    ///   d_end   = L - d_begin
    ///
    /// A pair is physical only if both reconstructed distances are inside
    /// the cell path length, within a configurable tolerance.

    bool physical_pair(const digit& begin_digit, const digit& end_digit, double length_mm, double v_mm_per_ns,
                       double tolerance_ns) {
      const double t_begin = begin_digit.tdc();
      const double t_end   = end_digit.tdc();

      if (!std::isfinite(t_begin) || !std::isfinite(t_end)) {
        return false;
      }

      const double dt_ns = t_begin - t_end;

      const double d_begin = 0.5 * (length_mm + v_mm_per_ns * dt_ns);
      const double d_end   = length_mm - d_begin;

      const double tolerance_mm = v_mm_per_ns * tolerance_ns;

      return d_begin >= -tolerance_mm && d_begin <= length_mm + tolerance_mm && d_end >= -tolerance_mm
          && d_end <= length_mm + tolerance_mm;
    }

  } // namespace

  cell_pair_builder::cell_pair_builder()
    : process({{"digi", "sand::ecal::digits_container"}, {"timeranges", "sand::reco::timeranges"}},
              {{"cell_pairs", "sand::ecal::cell_pairs"}}) {}

  void cell_pair_builder::configure(const ufw::config& cfg) {
    process::configure(cfg);

    m_print_output = cfg.value("print_output", m_print_output);

    m_pair_time_tolerance_ns = cfg.value("pair_time_tolerance_ns", m_pair_time_tolerance_ns);

    m_keep_incomplete = cfg.value("keep_incomplete", m_keep_incomplete);
  }

  void cell_pair_builder::run() {
    const auto& gi   = instance<sand::geoinfo>();
    const auto& ecal = gi.ecal();

    const auto& digi    = get<digits_container>("digi");
    const auto& windows = get<reco::timeranges>("timeranges");

    auto& pairs = set<cell_pairs>("cell_pairs");
    pairs.clear();

    // The iterator views refer to this local copy; preserve the original digit order.
    auto ordered = digi.digits;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const auto& lhs, const auto& rhs) { return lhs.t() < rhs.t(); });

    reco::timeslicer slicer(windows);
    const auto views = slicer.slice(ordered.cbegin(), ordered.cend());

    if (m_print_output) {
      UFW_INFO("ECAL cell_pair_builder spill {}: {} digits, {} windows, {} nonempty views",
               ufw::context::current()->id(), digi.digits.size(), windows.size(), views.size());
    }

    for (std::size_t iview = 0; iview < views.size(); ++iview) {
      const auto& view      = views.at(iview);
      const auto first_pair = pairs.size();

      /*
       * Group digits by physical ECal cell within this window only.
       *
       * We use cid.raw as the map key because operator< for cell_id is defined
       * in ecal_info.cpp. It just uses normal integer comparison.
       */

      std::map<std::uint32_t, grouped_cell_digits> by_cell;

      std::size_t idig = 0;
      for (auto it = view.begin(); it != view.end(); ++it, ++idig) {
        const auto& d = *it;

        const auto pmt = ecal.pmt(d.channel());

        if (!ecal.contains(pmt.cell_)) {
          UFW_ERROR("ECal cell_pair_builder: channel refers to an unknown cell: "
                    "cell = {}, region = {}, module = {}, row = {}, column = {}",
                    pmt.cell_.raw, static_cast<int>(pmt.cell_.region), static_cast<int>(pmt.cell_.module_number),
                    static_cast<int>(pmt.cell_.row), static_cast<int>(pmt.cell_.column));
        }

        auto& group = by_cell[pmt.cell_.raw];
        group.cid   = pmt.cell_;

        if (pmt.face_ == face_location::begin) {
          group.begin.push_back({&d, idig});
        } else if (pmt.face_ == face_location::end) {
          group.end.push_back({&d, idig});
        } else {
          UFW_ERROR("ECal cell_pair_builder: invalid PMT face {}", static_cast<int>(pmt.face_));
        }
      }
      if (m_print_output) {
        UFW_INFO("  view {}: [{:.17g}, {:.17g}) ns, best={:.17g} ns, digits={}, cells={}", iview, view.earliest(),
                 view.latest(), view.best(), idig, by_cell.size());
      }

      for (const auto& [raw_cid, group] : by_cell) {
        if (m_print_output) {
          UFW_INFO("    cell {}: begin digits={}, end digits={}", raw_cid, group.begin.size(), group.end.size());
        }
        /*
         * If the cell has only one readout side, no complete pair can be built.
         * In that case we do not need to access the ECal geometry at all.
         */
        if (group.begin.empty() || group.end.empty()) {
          if (m_keep_incomplete) {
            for (const auto& b : group.begin) {
              sand::ecal::cell_pair pair;
              pair.begin                             = *b.ptr;
              pair.begin_present                     = true;
              pair.cell_has_competing_complete_pairs = false;
              pair.n_complete_candidates_in_cell     = 0;

              pairs.push_back(std::move(pair));
            }

            for (const auto& e : group.end) {
              sand::ecal::cell_pair pair;
              pair.end                               = *e.ptr;
              pair.end_present                       = true;
              pair.cell_has_competing_complete_pairs = false;
              pair.n_complete_candidates_in_cell     = 0;

              pairs.push_back(std::move(pair));
            }
          }

          continue;
        }

        const auto& cell = ecal.at(group.cid);

        const double length_mm   = cell.total_pathlength();
        const double v_mm_per_ns = cell.get_fiber().light_velocity;

        if (!std::isfinite(length_mm) || length_mm <= 0.0 || !std::isfinite(v_mm_per_ns) || v_mm_per_ns <= 0.0) {
          UFW_ERROR("Invalid ECal cell timing geometry: cell = {}, region = {}, module = {}, row = {}, column = {}, "
                    "length = {} mm, light_velocity = {} mm/ns",
                    group.cid.raw, static_cast<int>(group.cid.region), static_cast<int>(group.cid.module_number),
                    static_cast<int>(group.cid.row), static_cast<int>(group.cid.column), length_mm, v_mm_per_ns);
        }

        std::set<std::size_t> begin_used_in_good_pair;
        std::set<std::size_t> end_used_in_good_pair;

        std::vector<sand::ecal::cell_pair> complete_pairs_in_cell;
        /*
         * Build all complete begin/end hypotheses.
         *
         * A unique matching is not chosen here. Pairs whose
         * reconstructed light-propagation distances are outside the cell are rejected.
         */
        for (const auto& b : group.begin) {
          for (const auto& e : group.end) {
            const bool is_physical = physical_pair(*b.ptr, *e.ptr, length_mm, v_mm_per_ns, m_pair_time_tolerance_ns);

            if (!is_physical) {
              continue;
            }

            sand::ecal::cell_pair pair;
            pair.begin         = *b.ptr;
            pair.end           = *e.ptr;
            pair.begin_present = true;
            pair.end_present   = true;

            complete_pairs_in_cell.push_back(std::move(pair));

            begin_used_in_good_pair.insert(b.index);
            end_used_in_good_pair.insert(e.index);
          }
        }

        /*
         * If more than one physically valid complete pair exists in this cell,
         * mark all of them as competing candidates.
         *
         * This is intentionally conservative: downstream clustering can use
         * only unambiguous complete pairs in the first pass and resolve the
         * ambiguous ones later using cluster compatibility.
         */
        const auto n_complete_pairs_in_cell = complete_pairs_in_cell.size();

        UFW_ASSERT(
            n_complete_pairs_in_cell <= std::numeric_limits<std::uint16_t>::max(),
            "ECal cell_pair_builder: digit window view {}, cell {} has {} valid complete pairs, which exceeds the "
            "uint16_t storage limit {}",
            iview, raw_cid, n_complete_pairs_in_cell, std::numeric_limits<std::uint16_t>::max());

        const bool has_competition                = n_complete_pairs_in_cell > 1;
        const auto n_valid_complete_pairs_in_cell = saturated_u16(n_complete_pairs_in_cell);

        for (auto& pair : complete_pairs_in_cell) {
          pair.cell_has_competing_complete_pairs = has_competition;
          pair.n_complete_candidates_in_cell     = n_valid_complete_pairs_in_cell;

          pairs.push_back(std::move(pair));
        }

        if (!m_keep_incomplete) {
          continue;
        }

        /*
         * Add incomplete hypotheses only for digits that were not part of
         * any accepted complete-pair hypothesis.
         *
         * These are not reconstructed here. They are kept for a later recovery
         * step that can use cluster-level information.
         */
        for (const auto& b : group.begin) {
          if (begin_used_in_good_pair.count(b.index) != 0) {
            continue;
          }

          sand::ecal::cell_pair pair;
          pair.begin                             = *b.ptr;
          pair.begin_present                     = true;
          pair.cell_has_competing_complete_pairs = has_competition;
          pair.n_complete_candidates_in_cell     = n_valid_complete_pairs_in_cell;

          pairs.push_back(std::move(pair));
        }

        for (const auto& e : group.end) {
          if (end_used_in_good_pair.count(e.index) != 0) {
            continue;
          }

          sand::ecal::cell_pair pair;
          pair.end                               = *e.ptr;
          pair.end_present                       = true;
          pair.cell_has_competing_complete_pairs = has_competition;
          pair.n_complete_candidates_in_cell     = n_valid_complete_pairs_in_cell;

          pairs.push_back(std::move(pair));
        }
      }

      if (m_print_output) {
        std::size_t n_complete  = 0;
        std::size_t n_ambiguous = 0;
        for (std::size_t ipair = first_pair; ipair < pairs.size(); ++ipair) {
          const auto& pair   = pairs.at(ipair);
          const auto raw_cid = ecal.pmt(pair.either().channel()).cell_.raw;
          if (pair.complete()) {
            ++n_complete;
            n_ambiguous += pair.ambiguous_complete();
            UFW_INFO("    pair {}: cell={}, complete, begin(channel={}, tdc={:.17g} ns, adc={}), "
                     "end(channel={}, tdc={:.17g} ns, adc={}), candidates={}, competing={}",
                     ipair, raw_cid, pair.begin.channel(), pair.begin.tdc(), pair.begin.adc(), pair.end.channel(),
                     pair.end.tdc(), pair.end.adc(), pair.n_complete_candidates_in_cell,
                     pair.cell_has_competing_complete_pairs);
          } else {
            const auto& digit = pair.either();
            UFW_INFO("    pair {}: cell={}, {}-only, channel={}, tdc={:.17g} ns, adc={}, "
                     "candidates={}, competing={}",
                     ipair, raw_cid, pair.has_begin() ? "begin" : "end", digit.channel(), digit.tdc(), digit.adc(),
                     pair.n_complete_candidates_in_cell, pair.cell_has_competing_complete_pairs);
          }
        }
        UFW_INFO("  view {} result: {} complete ({} ambiguous), {} incomplete, {} total pairs", iview, n_complete,
                 n_ambiguous, pairs.size() - first_pair - n_complete, pairs.size() - first_pair);
      }
    }

    if (m_print_output) {
      UFW_INFO("ECAL cell_pair_builder spill {}: {} output pairs", ufw::context::current()->id(), pairs.size());
    }
  }

} // namespace sand::ecal

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::cell_pair_builder)
