#include <edep_reader/edep_reader.hpp>
#include <geoinfo/grain_info.hpp>
#include <edep_reader/EDEPHit.h>

#include <grain/digi.h>
#include <grain/photons.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::test {

  class test_grain_optical_simulation : public ufw::process {
   public:
    test_grain_optical_simulation();
    void configure(const ufw::config& cfg) override;
    void run() override;
    void sort_corners(pos_3d&, pos_3d&);

   private:
    pos_3d m_min_LAr;
    pos_3d m_max_LAr;
    uint64_t m_stat_photon_tested;
  };

  void test_grain_optical_simulation::configure(const ufw::config& cfg) {
    UFW_DEBUG("test_grain_optical_simulation configured at: {}", fmt::ptr(this));
    const auto& gi = get<geoinfo>();
    auto xfrm      = gi.grain().transform();
    m_min_LAr      = pos_3d(-gi.grain().LAr_bbox());
    m_max_LAr      = pos_3d(gi.grain().LAr_bbox());
    sort_corners(m_min_LAr, m_max_LAr);
    UFW_DEBUG("Grain Argon volume corners {} and {}", m_min_LAr, m_max_LAr);
  }

  void test_grain_optical_simulation::sort_corners(pos_3d& a, pos_3d& b) {
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

  test_grain_optical_simulation::test_grain_optical_simulation() : process({{"hits", "sand::grain::hits"}}, {}) {
    UFW_INFO("Creating a test_grain_optical_simulation process at {}", fmt::ptr(this));
  }

  void test_grain_optical_simulation::run() {
    const auto& hits_in  = get<sand::grain::hits>("hits");
    edep_reader& edep    = ufw::context::current()->instance<edep_reader>();
    m_stat_photon_tested = 0;
    for (const auto& photon : hits_in.photons) {
      UFW_ASSERT(photon.pos.t() >= 0., "Non-physical photon arrival time");
      // in gdml geometry X and Z are swapped
      UFW_ASSERT((m_min_LAr.z() <= photon.origin.x() && photon.origin.x() <= m_max_LAr.z()
                  && m_min_LAr.y() <= photon.origin.y() && photon.origin.y() <= m_max_LAr.y()
                  && m_min_LAr.x() <= photon.origin.z() && photon.origin.z() <= m_max_LAr.x()),
                 "Photon origin outside LAr volume: {}", photon.origin);
      UFW_ASSERT(photon.p.E() >= 0., "Non-physical photon energy");
      auto trajectory = edep.GetTrajectoryWithHitId(photon.get());
      auto& hit       = trajectory->GetHitWithId(photon.get());
      UFW_ASSERT(hit.GetSecondaryDeposit() >= 1.0e-5, "Energy deposit too small to have produced scintillation.");
      m_stat_photon_tested++;
    }
    UFW_INFO("{} Photon hits tested", m_stat_photon_tested);
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::test_grain_optical_simulation)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::test_grain_optical_simulation)
