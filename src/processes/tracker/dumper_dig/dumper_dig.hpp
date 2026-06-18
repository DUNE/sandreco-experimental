// #include <edep_reader/edep_reader.hpp>
// #include <geoinfo/drift_info.hpp>
// #include <geoinfo/geoinfo.hpp>
// #include <geoinfo/stt_info.hpp>
// #include <geoinfo/tracker_info.hpp>
// #include <root_tgeomanager/root_tgeomanager.hpp>
// #include <tracker/digi.h>
// #include <tracker/cluster_container.h>

// #include <ufw/config.hpp>
// #include <ufw/context.hpp>
// #include <ufw/data.hpp>
// #include <ufw/factory.hpp>
// #include <ufw/process.hpp>

#include <TFile.h>
#include <TTree.h>

namespace sand::tracker {

  class dumper_dig : public ufw::process {
   public:
    dumper_dig();
    ~dumper_dig();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    std::unique_ptr<TFile> _fout;
    TTree* _tree;
    std::vector<double> _adc;
    std::vector<double> _tdc;
    std::vector<double> _w_x;
    std::vector<double> _w_y;
    std::vector<double> _w_z;
    std::vector<double> _w_dx;
    std::vector<double> _w_dy;
    std::vector<double> _w_dz;
  };

} // namespace sand::tracker

UFW_REGISTER_PROCESS(sand::tracker::dumper_dig)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::tracker::dumper_dig)