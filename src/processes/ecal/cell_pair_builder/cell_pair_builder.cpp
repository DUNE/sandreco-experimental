#include <cell_pair_builder.hpp>

#include <ecal/cell_pair.h>
#include <ecal/cell_pair_slice.h>
#include <ecal/digit.h>
#include <ecal/digit_slice.h>

#include <geoinfo/ecal_info.hpp>
#include <geoinfo/geoinfo.hpp>

#include <ufw/factory.hpp>

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
    : process({{"digit_slices", "sand::ecal::digit_slices_container"}},
              {{"cell_pair_slices", "sand::ecal::cell_pair_slices_container"}}) {}

  void cell_pair_builder::configure(const ufw::config& cfg) {
    process::configure(cfg);

    m_pair_time_tolerance_ns = cfg.value("pair_time_tolerance_ns", m_pair_time_tolerance_ns);

    m_keep_incomplete = cfg.value("keep_incomplete", m_keep_incomplete);
  }

  void cell_pair_builder::run() {
    const auto& gi   = get<sand::geoinfo>();
    const auto& ecal = gi.ecal();

    const auto& digit_slices = get<sand::ecal::digit_slices_container>("digit_slices");

    auto& cell_pair_slices = set<sand::ecal::cell_pair_slices_container>("cell_pair_slices");

    cell_pair_slices.collection.clear();
    cell_pair_slices.collection.reserve(digit_slices.collection.size());

    for (std::size_t islice = 0; islice < digit_slices.collection.size(); ++islice) {
      const auto& digit_slice = digit_slices.collection.at(islice);
      auto& out_slice         = cell_pair_slices.collection.emplace_back();

      /*
       * Group digits by physical ECal cell.
       *
       * We use cid.raw as the map key because operator< for cell_id is defined
       * in ecal_info.cpp. It just uses normal integer comparison.
       */

      std::map<std::uint32_t, grouped_cell_digits> by_cell;

      for (std::size_t idig = 0; idig < digit_slice.size(); ++idig) {
        const auto& d = digit_slice.at(idig);

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
      for (const auto& [raw_cid, group] : by_cell) {
        /*
         * If the cell has only one readout side, no complete pair can be built.
         * In that case we do not need to access the ECal geometry at all.
         */
        if (group.begin.empty() || group.end.empty()) {
          if (m_keep_incomplete) {
            for (const auto& b : group.begin) {
              sand::ecal::cell_pair pair;
              pair.begin                             = *b.ptr;
              pair.cell_has_competing_complete_pairs = false;
              pair.n_complete_candidates_in_cell     = 0;

              out_slice.push_back(std::move(pair));
            }

            for (const auto& e : group.end) {
              sand::ecal::cell_pair pair;
              pair.end                               = *e.ptr;
              pair.cell_has_competing_complete_pairs = false;
              pair.n_complete_candidates_in_cell     = 0;

              out_slice.push_back(std::move(pair));
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
            pair.begin = *b.ptr;
            pair.end   = *e.ptr;

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

        UFW_ASSERT(n_complete_pairs_in_cell <= std::numeric_limits<std::uint16_t>::max(),
                   "ECal cell_pair_builder: slice {}, cell {} has {} valid complete pairs, which exceeds the "
                   "uint16_t storage limit {}",
                   islice, raw_cid, n_complete_pairs_in_cell, std::numeric_limits<std::uint16_t>::max());

        const bool has_competition                = n_complete_pairs_in_cell > 1;
        const auto n_valid_complete_pairs_in_cell = saturated_u16(n_complete_pairs_in_cell);

        for (auto& pair : complete_pairs_in_cell) {
          pair.cell_has_competing_complete_pairs = has_competition;
          pair.n_complete_candidates_in_cell     = n_valid_complete_pairs_in_cell;

          out_slice.push_back(std::move(pair));
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
          pair.cell_has_competing_complete_pairs = has_competition;
          pair.n_complete_candidates_in_cell     = n_valid_complete_pairs_in_cell;

          out_slice.push_back(std::move(pair));
        }

        for (const auto& e : group.end) {
          if (end_used_in_good_pair.count(e.index) != 0) {
            continue;
          }

          sand::ecal::cell_pair pair;
          pair.end                               = *e.ptr;
          pair.cell_has_competing_complete_pairs = has_competition;
          pair.n_complete_candidates_in_cell     = n_valid_complete_pairs_in_cell;

          out_slice.push_back(std::move(pair));
        }
      }
    }

    UFW_ASSERT(cell_pair_slices.collection.size() == digit_slices.collection.size(),
               "ECal cell_pair_builder: output slices ({}) do not match input slices ({})",
               cell_pair_slices.collection.size(), digit_slices.collection.size());
  }

} // namespace sand::ecal

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::cell_pair_builder)
