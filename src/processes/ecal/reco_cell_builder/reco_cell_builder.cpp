#include <common/version.h>
#include <reco_cell_builder.hpp>

#include <ecal/cell_pair_slice.h>
#include <ecal/reco_cell_slice.h>

#include <geoinfo/ecal_info.hpp>
#include <geoinfo/geoinfo.hpp>

#include <ufw/factory.hpp>

#include <cmath>

namespace sand::ecal {

  namespace {

    bool finite_positive(double value) { return std::isfinite(value) && value > 0.0; }
    bool finite_non_negative(double value) { return std::isfinite(value) && value >= 0.0; }

  } // namespace

  reco_cell_builder::reco_cell_builder()
    : process({{"cell_pair_slices", "sand::ecal::cell_pair_slices_container"}},
              {{"reco_cell_slices", "sand::ecal::reco_cell_slices_container"}}) {}

  void reco_cell_builder::configure(const ufw::config& cfg) {
    process::configure(cfg);

    m_adc_to_pe                    = cfg.value("adc_to_pe", m_adc_to_pe);
    m_light_yield_pe_per_mev       = cfg.value("light_yield_pe_per_mev", m_light_yield_pe_per_mev);
    m_apply_attenuation_correction = cfg.value("apply_attenuation_correction", m_apply_attenuation_correction);

    UFW_ASSERT(finite_positive(m_adc_to_pe), "ECal reco_cell_builder: adc_to_pe must be positive, got {}", m_adc_to_pe);
    UFW_ASSERT(finite_positive(m_light_yield_pe_per_mev),
               "ECal reco_cell_builder: light_yield_pe_per_mev must be positive, got {}", m_light_yield_pe_per_mev);
  }

  void reco_cell_builder::run() {
    const auto& ecal = get<sand::geoinfo>().ecal();
    const auto& cell_pair_slices = get<sand::ecal::cell_pair_slices_container>("cell_pair_slices");

    auto& reco_cell_slices = set<sand::ecal::reco_cell_slices_container>("reco_cell_slices");

    reco_cell_slices.collection.clear();
    reco_cell_slices.collection.reserve(cell_pair_slices.collection.size());

    for (std::size_t islice = 0; islice < cell_pair_slices.collection.size(); ++islice) {
      const auto& cell_pair_slice = cell_pair_slices.collection.at(islice);
      auto& out_slice             = reco_cell_slices.collection.emplace_back();
      out_slice.reserve(cell_pair_slice.size());

      for (const auto& pair : cell_pair_slice) {
        // Step 1: build reco cells only from complete, unambiguous pairs.
        if (!pair.complete()) {
          continue;
        }

        if (!pair.unambiguous_complete()) {
          continue;
        }

        const auto& begin_digit = *pair.begin;
        const auto& end_digit   = *pair.end;

        const auto begin_pmt = ecal.pmt(begin_digit.channel());
        const auto end_pmt   = ecal.pmt(end_digit.channel());

        using face_location = sand::geoinfo::ecal_info::face_location;

        if (begin_pmt.face_ != face_location::begin || end_pmt.face_ != face_location::end
            || begin_pmt.cell_.raw != end_pmt.cell_.raw) {
          UFW_ASSERT(false, "ECal reco_cell_builder: invalid complete pair: begin face = {}, end face = {}, "
                     "begin cell = {:x}, end cell = {:x}",
                     static_cast<int>(begin_pmt.face_), static_cast<int>(end_pmt.face_), begin_pmt.cell_.raw,
                     end_pmt.cell_.raw);
        }

        const auto& cell = ecal.at(begin_pmt.cell_);

        const double length_mm   = cell.total_pathlength();
        const double v_mm_per_ns = cell.get_fiber().light_velocity;
        const double t_begin     = begin_digit.tdc();
        const double t_end       = end_digit.tdc();
        const double pe_begin    = begin_digit.adc() * m_adc_to_pe;
        const double pe_end      = end_digit.adc() * m_adc_to_pe;

        if (!finite_positive(length_mm) || !finite_positive(v_mm_per_ns) || !std::isfinite(t_begin)
            || !std::isfinite(t_end) || !finite_non_negative(pe_begin) || !finite_non_negative(pe_end)) {
          UFW_ASSERT(false, "ECal reco_cell_builder: invalid complete pair: length = {}, light_velocity = {} mm/ns, "
                     "t_begin = {}, t_end = {}, pe_begin = {}, pe_end = {}",
                     length_mm, v_mm_per_ns, t_begin, t_end, pe_begin, pe_end);
        }

        const double half_length = 0.5 * length_mm;
        const double d_begin     = half_length + 0.5 * v_mm_per_ns * (t_begin - t_end);
        const double d_end       = length_mm - d_begin;

        if (!std::isfinite(d_begin) || !std::isfinite(d_end)) {
          UFW_ASSERT(false, "ECal reco_cell_builder: invalid complete pair: d_begin = {}, d_end = {}",
                     d_begin, d_end);
          continue;
        }

        double corrected_pe_begin = pe_begin;
        double corrected_pe_end   = pe_end;

        if (m_apply_attenuation_correction) {
          const double attenuation_begin = cell.attenuation(d_begin);
          const double attenuation_end   = cell.attenuation(d_end);

          if (!finite_positive(attenuation_begin) || !finite_positive(attenuation_end)) {
            UFW_ASSERT(false, "ECal reco_cell_builder: invalid complete pair: attenuation_begin = {}, attenuation_end = {}",
                       attenuation_begin, attenuation_end);
            continue;
          }

          corrected_pe_begin /= attenuation_begin; // retrieved PE counts corrected for fiber attenuation
          corrected_pe_end /= attenuation_end;
        }

        const double energy_mev = 0.5 * (corrected_pe_begin + corrected_pe_end) / m_light_yield_pe_per_mev;
        const double time_ns    = 0.5 * (t_begin + t_end - length_mm / v_mm_per_ns);

        if (!std::isfinite(energy_mev) || !std::isfinite(time_ns)) {
          UFW_ASSERT(false, "ECal reco_cell_builder: invalid complete pair: energy = {}, time = {}",
                     energy_mev, time_ns);
          continue;
        }

        const auto position = cell.offset2position(d_begin - half_length);

        auto& reco_cell = out_slice.emplace_back();
        static_cast<sand::ecal::cell_pair&>(reco_cell) = pair;
        reco_cell.position                             = position;
        reco_cell.time                                 = sand::reco::timerange(time_ns);
        reco_cell.e                                    = energy_mev;
        reco_cell.d_begin                              = d_begin;
        reco_cell.d_end                                = d_end;
        reco_cell.originally_incomplete                = false;
      }
    }

    UFW_ASSERT(reco_cell_slices.collection.size() == cell_pair_slices.collection.size(),
               "ECal reco_cell_builder: output slices ({}) do not match input slices ({})",
               reco_cell_slices.collection.size(), cell_pair_slices.collection.size());

  }

} // namespace sand::ecal

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::reco_cell_builder)
