#include <edep_reader/edep_reader.hpp>
#include <geoinfo/grain_info.hpp>
#include <common/array.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>
#include <utility>

namespace sand::test {

  class grain_energy_in_fiducial : public ufw::process {
   public:
    grain_energy_in_fiducial();
    void configure(const ufw::config& cfg) override;
    void run() override;

    static void sort_corners(pos_3d&, pos_3d&);

   private:
    pos_3d m_min_fiducial;
    pos_3d m_max_fiducial;
    pos_3d m_min_LAr;
    pos_3d m_max_LAr;
  };

  grain_energy_in_fiducial::grain_energy_in_fiducial() : process({}, {{"inside", "sand::array<double>"},
                                                                      {"outside", "sand::array<double>"}}) {
  }

  void grain_energy_in_fiducial::configure(const ufw::config& cfg) {
    const auto& gi = get<geoinfo>();
    auto xfrm = gi.grain().transform();
    UFW_INFO("Grain fiducial volume of size {} at {}", gi.grain().fiducial_bbox(), xfrm);
    m_min_fiducial = xfrm * pos_3d(-gi.grain().fiducial_bbox());
    m_max_fiducial = xfrm * pos_3d(gi.grain().fiducial_bbox());
    sort_corners(m_min_fiducial, m_max_fiducial);
    m_min_LAr = xfrm * pos_3d(-gi.grain().LAr_bbox());
    m_max_LAr = xfrm * pos_3d(gi.grain().LAr_bbox());
    sort_corners(m_min_LAr, m_max_LAr);
    UFW_INFO("Grain fiducial volume corners {} and {}", m_min_fiducial, m_max_fiducial);
    UFW_INFO("Grain Argon volume corners {} and {}", m_min_LAr, m_max_LAr);
  }

  void grain_energy_in_fiducial::sort_corners(pos_3d& a, pos_3d& b) {
    double tmp;
    if (a.x() > b.x()) {
      tmp = b.x();
      b.SetX(a.x());
      a.SetX(tmp);
    }
    if (a.y() > b.y()) {
      tmp = b.y();
      b.SetY(a.y());
      a.SetY(tmp);
    }
    if (a.z() > b.z()) {
      tmp = b.z();
      b.SetZ(a.z());
      a.SetZ(tmp);
    }
  }


  void grain_energy_in_fiducial::run() {
    const auto& tree = get<sand::edep_reader>();
    double inside = 0.0;
    double outside = 0.0;
    double total = 0.0;
    for (auto trj_it = tree.begin(); trj_it != tree.end(); trj_it++) {
      if (trj_it->GetHitMap().find(component::GRAIN) != trj_it->GetHitMap().end()) {
        for (auto& dep : trj_it->GetHitMap().at(component::GRAIN)) {
          auto hit = ((dep.GetStart() + dep.GetStop()) / 2.0).Vect();
          if (m_min_fiducial.x() <= hit.x() && hit.x() <= m_max_fiducial.x() &&
              m_min_fiducial.y() <= hit.y() && hit.y() <= m_max_fiducial.y() &&
              m_min_fiducial.z() <= hit.z() && hit.z() <= m_max_fiducial.z()) {
            inside += dep.GetSecondaryDeposit();
          } else if (m_min_LAr.x() <= hit.x() && hit.x() <= m_max_LAr.x() &&
                     m_min_LAr.y() <= hit.y() && hit.y() <= m_max_LAr.y() &&
                     m_min_LAr.z() <= hit.z() && hit.z() <= m_max_LAr.z()) {
            outside += dep.GetSecondaryDeposit();
          } else {
            UFW_WARN("Energy deposit for component GRAIN found outside GRAIN bounding box at {}.", hit);
          }
          total += dep.GetSecondaryDeposit();
        }
      }
    }
    UFW_INFO("Found {} MeV inside, {} MeV outside, {} MeV total", inside, outside, total);
    //UFW_ASSERT(std::abs(total - outside - inside) < 1.e-6, "Total does not match");
    set<sand::array<double>>("inside").values.push_back(inside);
    set<sand::array<double>>("outside").values.push_back(outside);
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::grain_energy_in_fiducial)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::grain_energy_in_fiducial)
