#include <random>
#include <data.hpp>
#include <factory.hpp>

#include <caf_srsand.hpp>
#include <produce_srsand.hpp>

UFW_REGISTER_PROCESS(sand::process::produce_srsand)

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::process::produce_srsand)

namespace sand {

  namespace process {

    void produce_srsand::configure (const ufw::config& cfg) {
      m_seed = cfg.value("random_seed", 0ul);
    }

    ufw::data_list produce_srsand::products() const {
      return ufw::data_list{{"output", "caf::SRSANDInt"}};
    }

    ufw::data_list produce_srsand::requirements() const {
      return ufw::data_list{};
    }

    void produce_srsand::run(const ufw::data_set& input, ufw::data_set& output) {
      auto& sandint = ufw::data_cast<caf::SRSANDInt>(*output.at("output"));
      std::mt19937 engine(m_seed);
      std::uniform_int_distribution<std::size_t> u100(0, 100);
      std::uniform_real_distribution<double> volume(-400., 400);
      std::uniform_real_distribution<double> phi(0., 2. * M_PI);
      std::uniform_real_distribution<double> costheta(0., 1.);
      std::chi_squared_distribution<double> egev(5.);
      auto ntracks = u100(engine);
      for (auto i = ntracks; i; --i) {
        caf::SRVector3D vtx(volume(engine), volume(engine), volume(engine));
        double len = std::abs(volume(engine)) + 2.0;
        double z = costheta(engine);
        double r = 1 - z;
        double p = phi(engine);
        caf::SRVector3D dir(r * std::cos(p), r * std::sin(p), z);
        dir = dir.Unit();
        double e = egev(engine) * 1000.;
        double evis = len / 400. * e;
        caf::SRTrack trk;
        trk.start = vtx;
        trk.end = vtx + caf::SRVector3D(len * dir.x, len * dir.y, len * dir.z);
        trk.dir = dir;
        trk.enddir = dir;
        trk.Evis = len / 400. * e;
        trk.qual = 1;
        trk.len_gcm2 = len;
        trk.len_cm   = len;
        trk.E = e;
        sandint.tracks.push_back(trk);
      }
      sandint.ntracks = sandint.tracks.size();
    }

  }

}
