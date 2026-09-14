#include <common/version.h>
#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

// #include <edep_reader/edep_reader.hpp>
// #include <geoinfo/drift_info.hpp>
#include <geoinfo/geoinfo.hpp>
// #include <geoinfo/stt_info.hpp>
#include <geoinfo/tracker_info.hpp>
// #include <root_tgeomanager/root_tgeomanager.hpp>
#include <tracker/digi.h>
// #include <tracker/cluster_container.h>

#include <dumper_dig.hpp>

namespace sand::tracker {

  void dumper_dig::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_filepath = cfg.at("filepath");
    UFW_DEBUG("Output filepath: {}", m_filepath);
    _fout = std::unique_ptr<TFile>(new TFile(m_filepath.c_str(), "RECREATE"));
    _tree = new TTree("digi_tree", "Digi Tree");
    _tree->Branch("adc", &_adc);
    _tree->Branch("tdc", &_tdc);
    _tree->Branch("w_x", &_w_x);
    _tree->Branch("w_y", &_w_y);
    _tree->Branch("w_z", &_w_z);
    _tree->Branch("w_dx", &_w_dx);
    _tree->Branch("w_dy", &_w_dy);
    _tree->Branch("w_dz", &_w_dz);
  }

  dumper_dig::dumper_dig() : process({{"digi", "sand::tracker::digi"}}, {}) {
    UFW_DEBUG("Creating dumper_dig process at {}", fmt::ptr(this));
  }

  dumper_dig::~dumper_dig() {
    UFW_DEBUG("Destructing dumper_dig process at {}", fmt::ptr(this));
    _fout->cd();
    _fout->Write();
    _fout->Close();
  }

  void dumper_dig::run() {
    const auto& digi = get<sand::tracker::digi>("digi");
    const auto& gi   = get<geoinfo>();
    _adc.clear();
    _tdc.clear();
    _w_x.clear();
    _w_y.clear();
    _w_z.clear();
    _w_dx.clear();
    _w_dy.clear();
    _w_dz.clear();
    for (const auto& signal : digi.signals) {
      _adc.push_back(signal.adc());
      _tdc.push_back(signal.tdc());
      auto w   = gi.tracker().wire_at(signal.channel());
      auto pos = w.head;
      auto dir = w.direction();
      dir /= dir.r();
      _w_x.push_back(pos.x());
      _w_y.push_back(pos.y());
      _w_z.push_back(pos.z());
      _w_dx.push_back(dir.x());
      _w_dy.push_back(dir.y());
      _w_dz.push_back(dir.z());
    }
    _tree->Fill();
  }

} // namespace sand::tracker
