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

    bool valid_face(face_location face) { return face == face_location::begin || face == face_location::end; }

    std::size_t n_endcap_columns(module_t module_number) {
      switch (module_number) {
      case 0:
      case 1:
      case 16:
      case 17:
        return 6;

      case 12:
      case 13:
      case 14:
      case 15:
      case 28:
      case 29:
      case 30:
      case 31:
        return 2;

      default:
        return 3;
      }
    }

    bool valid_cell_id(cell_id cid) {
      using region_t = sand::geo_id::region_t;

      const auto module = static_cast<std::size_t>(cid.module_number);
      const auto row    = static_cast<std::size_t>(cid.row);
      const auto column = static_cast<std::size_t>(cid.column);

      /*
       * ECal cells are constructed in geoinfo::ecal_info::construct_module_cells()
       * with irow = 0..4.
       */
      if (row >= 5) {
        return false;
      }

      /*
       * Barrel:
       *
       * geoinfo::ecal_info::barrel_module_cells() uses:
       *
       *   static std::vector<double> col_widths(12, 1.);
       *
       * so there are 12 columns.
       *
       * The usual KLOE barrel has 24 modules, indexed 0..23.
       */
      if (cid.region == region_t::BARREL) {
        return module < 24 && column < 12;
      }

      /*
       * Endcaps:
       *
       * geoinfo::ecal_info::endcap_module_cells() constructs modules 0..31.
       *
       * Modules 0, 1, 16, 17 have 6 columns.
       * Modules 12..15 and 28..31 have 2 columns.
       * All other modules have 3 columns.
       */
      if (cid.region == region_t::ENDCAP_A || cid.region == region_t::ENDCAP_B) {
        if (module >= 32) {
          return false;
        }

        return column < n_endcap_columns(cid.module_number);
      }

      return false;
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
              {{"cell_pair_slices", "sand::ecal::cell_pair_slices_container"}}) {
    UFW_DEBUG("Creating ECal cell_pair_builder process at {}", fmt::ptr(this));
  }

  void cell_pair_builder::configure(const ufw::config& cfg) {
    process::configure(cfg);

    m_pair_time_tolerance_ns = cfg.value("pair_time_tolerance_ns", m_pair_time_tolerance_ns);

    m_keep_incomplete = cfg.value("keep_incomplete", m_keep_incomplete);

    UFW_INFO("Configured ECal cell_pair_builder: pair_time_tolerance_ns = {}, keep_incomplete = {}",
             m_pair_time_tolerance_ns, m_keep_incomplete);
  }

  void cell_pair_builder::run() {
    const auto& gi   = get<sand::geoinfo>();
    const auto& ecal = gi.ecal();

    const auto& digit_slices = get<sand::ecal::digit_slices_container>("digit_slices");

    auto& cell_pair_slices = set<sand::ecal::cell_pair_slices_container>("cell_pair_slices");

    cell_pair_slices.collection.clear();
    cell_pair_slices.collection.reserve(digit_slices.collection.size());

    UFW_INFO("ECal cell_pair_builder: input digit slices = {}", digit_slices.collection.size());

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

      std::size_t n_digits_invalid_face = 0;
      std::size_t n_digits_invalid_cell = 0;

      for (std::size_t idig = 0; idig < digit_slice.size(); ++idig) {
        const auto& d = digit_slice.at(idig);

        const auto pmt = ecal.pmt(d.channel());

        if (!valid_face(pmt.face_)) {
          ++n_digits_invalid_face;

          UFW_WARN("Ecal cell_pair_builder: skipping digit {} in slice {} with invalid face {}; channel raw = {}", idig,
                   islice, static_cast<int>(pmt.face_), d.channel().raw);

          continue;
        }

        if (!valid_cell_id(pmt.cell_)) {
          ++n_digits_invalid_cell;

          UFW_WARN("Ecal cell_pair_builder: skipping digit {} in slice {} with invalid decoded ECal cell: "
                   "cell raw = {}, region = {}, module = {}, row = {}, column = {}, face = {}, channel raw = {}",
                   idig, islice, pmt.cell_.raw, static_cast<int>(pmt.cell_.region),
                   static_cast<int>(pmt.cell_.module_number), static_cast<int>(pmt.cell_.row),
                   static_cast<int>(pmt.cell_.column), static_cast<int>(pmt.face_), d.channel().raw);

          continue;
        }

        auto& group = by_cell[pmt.cell_.raw];
        group.cid   = pmt.cell_;

        const auto ch = d.channel();

        const char* face_name = "unknown";
        if (pmt.face_ == face_location::begin) {
          face_name = "begin";
        } else if (pmt.face_ == face_location::end) {
          face_name = "end";
        }

        UFW_DEBUG("ECAL channel debug: slice = {}, digit = {}, "
                  "channel.raw = {:#018x}, channel.subdetector = {}, channel.link = {}, channel.channel = {:#010x}, "
                  "pmt.cell.raw = {:#010x}, region = {}, module = {}, row = {}, column = {}, face = {} ({})",
                  islice, idig, ch.raw, static_cast<int>(ch.subdetector), static_cast<int>(ch.link), ch.channel,
                  pmt.cell_.raw, static_cast<int>(pmt.cell_.region), static_cast<int>(pmt.cell_.module_number),
                  static_cast<int>(pmt.cell_.row), static_cast<int>(pmt.cell_.column), static_cast<int>(pmt.face_),
                  face_name);

        if (pmt.face_ == face_location::begin) {
          group.begin.push_back({&d, idig});
        } else {
          group.end.push_back({&d, idig});
        }
      }

      std::size_t n_complete_candidates        = 0;
      std::size_t n_unphysical_pairs_removed   = 0;
      std::size_t n_cells_with_unphysical_pair = 0;
      std::size_t n_complete_kept              = 0;
      std::size_t n_ambiguous_complete         = 0;
      std::size_t n_unambiguous_complete       = 0;
      std::size_t n_incomplete                 = 0;
      std::size_t n_one_sided_cells            = 0;
      std::size_t n_grouped_invalid_cells      = 0;

      for (const auto& [raw_cid, group] : by_cell) {
        /*
         * If the cell has only one readout side, no complete pair can be built.
         * In that case we do not need to access the ECal geometry at all.
         */
        if (group.begin.empty() || group.end.empty()) {
          ++n_one_sided_cells;

          if (m_keep_incomplete) {
            for (const auto& b : group.begin) {
              sand::ecal::cell_pair pair;
              pair.begin                             = *b.ptr;
              pair.cell_has_competing_complete_pairs = false;
              pair.n_complete_candidates_in_cell     = 0;

              out_slice.push_back(std::move(pair));
              ++n_incomplete;
            }

            for (const auto& e : group.end) {
              sand::ecal::cell_pair pair;
              pair.end                               = *e.ptr;
              pair.cell_has_competing_complete_pairs = false;
              pair.n_complete_candidates_in_cell     = 0;

              out_slice.push_back(std::move(pair));
              ++n_incomplete;
            }
          }

          continue;
        }

        /*
         * This should already be true because every digit was checked during
         * grouping. Keep the guard anyway, so ecal.at() is never called for
         * invalid decoded IDs.
         */

        if (!valid_cell_id(group.cid)) {
          ++n_grouped_invalid_cells;

          UFW_WARN("ECal cell_pair_builder: skipping grouped cell with invalid decoded ECal cell: "
                   "cell raw = {}, region = {}, module = {}, row = {}, column = {}",
                   group.cid.raw, static_cast<int>(group.cid.region), static_cast<int>(group.cid.module_number),
                   static_cast<int>(group.cid.row), static_cast<int>(group.cid.column));

          continue;
        }

        UFW_DEBUG("ECal cell_pair_builder: accessing geoinfo cell in slice {}: "
                  "cell raw = {}, region = {}, module = {}, row = {}, column = {}, begin digits = {}, end digits = {}",
                  islice, group.cid.raw, static_cast<int>(group.cid.region), static_cast<int>(group.cid.module_number),
                  static_cast<int>(group.cid.row), static_cast<int>(group.cid.column), group.begin.size(),
                  group.end.size());

        const auto& cell = ecal.at(group.cid);

        const double length_mm   = cell.total_pathlength();
        const double v_mm_per_ns = cell.get_fiber().light_velocity;

        if (!std::isfinite(length_mm) || length_mm <= 0.0 || !std::isfinite(v_mm_per_ns) || v_mm_per_ns <= 0.0) {
          UFW_ERROR("Invalid ECal cell timing geometry: cell = {}, region = {}, module = {}, row = {}, column = {}, "
                    "length = {} mm, light_velocity = {} mm/ns",
                    group.cid.raw, static_cast<int>(group.cid.region), static_cast<int>(group.cid.module_number),
                    static_cast<int>(group.cid.row), static_cast<int>(group.cid.column), length_mm, v_mm_per_ns);
        }

        const auto begin_side = cell.side(face_location::begin);
        const auto end_side   = cell.side(face_location::end);

        UFW_DEBUG("ECal cell_pair_builder: slice {}, cell {}: begin digits = {}, end digits = {}, begin side = {}, end "
                  "side = {}",
                  islice, raw_cid, group.begin.size(), group.end.size(), begin_side, end_side);

        std::set<std::size_t> begin_used_in_good_pair;
        std::set<std::size_t> end_used_in_good_pair;

        std::vector<sand::ecal::cell_pair> complete_pairs_in_cell;
        bool cell_has_unphysical_pair = false;

        /*
         * Build all complete begin/end hypotheses.
         *
         * A unique matching is not chosen here. Pairs whose
         * reconstructed light-propagation distances are outside the cell are rejected.
         */
        for (const auto& b : group.begin) {
          for (const auto& e : group.end) {
            ++n_complete_candidates;

            const double t_begin = b.ptr->tdc();
            const double t_end   = e.ptr->tdc();
            const double dt      = t_begin - t_end;

            const double max_dt = length_mm / v_mm_per_ns;

            const double d_begin = 0.5 * (length_mm + v_mm_per_ns * dt);
            const double d_end   = length_mm - d_begin;

            const bool is_physical = physical_pair(*b.ptr, *e.ptr, length_mm, v_mm_per_ns, m_pair_time_tolerance_ns);

            const char* region_name = "unknown";

            if (group.cid.region == sand::geo_id::region_t::BARREL) {
              region_name = "barrel";
            } else if (group.cid.region == sand::geo_id::region_t::ENDCAP_A) {
              region_name = "endcap_A";
            } else if (group.cid.region == sand::geo_id::region_t::ENDCAP_B) {
              region_name = "endcap_B";
            }

            UFW_DEBUG("ECal pair timing debug: slice = {}, cell = {}, region = {}, module = {}, row = {}, column = {}, "
                      "begin digit = {}, end digit = {}, "
                      "t_begin = {} ns, t_end = {} ns, dt = {} ns, max_dt = {} ns, "
                      "length = {} mm, v = {} mm/ns, d_begin = {} mm, d_end = {} mm, physical = {}",
                      islice, group.cid.raw, region_name, static_cast<int>(group.cid.module_number),
                      static_cast<int>(group.cid.row), static_cast<int>(group.cid.column), b.index, e.index, t_begin,
                      t_end, dt, max_dt, length_mm, v_mm_per_ns, d_begin, d_end, is_physical);

            if (!is_physical) {
              ++n_unphysical_pairs_removed;
              cell_has_unphysical_pair = true;
              continue;
            }

            sand::ecal::cell_pair pair;
            pair.begin = *b.ptr;
            pair.end   = *e.ptr;

            complete_pairs_in_cell.push_back(std::move(pair));

            begin_used_in_good_pair.insert(b.index);
            end_used_in_good_pair.insert(e.index);

            ++n_complete_kept;
          }
        }

        if (cell_has_unphysical_pair) {
          ++n_cells_with_unphysical_pair;
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

        if (has_competition) {
          UFW_DEBUG("ECal cell_pair_builder: slice {}, cell {} has {} competing complete pairs", islice, raw_cid,
                    n_complete_pairs_in_cell);
        }

        for (auto& pair : complete_pairs_in_cell) {
          pair.cell_has_competing_complete_pairs = has_competition;
          pair.n_complete_candidates_in_cell     = n_valid_complete_pairs_in_cell;

          if (has_competition) {
            ++n_ambiguous_complete;
          } else {
            ++n_unambiguous_complete;
          }

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
          ++n_incomplete;
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
          ++n_incomplete;
        }
      }

      UFW_INFO("ECal cell_pair_builder: slice {}: input digits = {}, grouped cells = {}, invalid-face digits = {}, "
               "invalid-cell digits = {}, one-sided cells = {}, grouped-invalid cells = {}, complete candidates = {}, "
               "kept complete = {}, unambiguous complete = {}, ambiguous complete = {}, "
               "unphysical complete pairs removed = {}, cells with unphysical complete pairs = {}, incomplete = {}, "
               "output pairs = {}",
               islice, digit_slice.size(), by_cell.size(), n_digits_invalid_face, n_digits_invalid_cell,
               n_one_sided_cells, n_grouped_invalid_cells, n_complete_candidates, n_complete_kept,
               n_unambiguous_complete, n_ambiguous_complete, n_unphysical_pairs_removed, n_cells_with_unphysical_pair,
               n_incomplete, out_slice.size());
    }

    UFW_ASSERT(cell_pair_slices.collection.size() == digit_slices.collection.size(),
               "ECal cell_pair_builder: output slices ({}) do not match input slices ({})",
               cell_pair_slices.collection.size(), digit_slices.collection.size());
  }

} // namespace sand::ecal

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::cell_pair_builder)