#include "fast_reco.hpp"

#include <ufw/factory.hpp>

namespace sand::common {

  fast_reco::fast_reco()
    : process{{{"in_truth", "sand::caf::truth_branch_wrapper"}},
              {{"out_common", "sand::caf::common_reco_branch"}, {"out_nd", "sand::caf::nd_reco_branch"}}} {}

  void fast_reco::run() {}

} // namespace sand::common

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::common::fast_reco);
